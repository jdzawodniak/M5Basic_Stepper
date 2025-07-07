// src/main.cpp

#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <XboxSeriesXControllerESP32_asukiaaa.hpp>

using namespace XboxSeriesXControllerESP32_asukiaaa;

//—————————————————————————————————————————————
// Configuration & Globals
//—————————————————————————————————————————————

Preferences         preferences;
Core                xbox;

// TB6600 #1 pins
static const int    ENABLE1_PIN   = 5;
static const int    DIR1_PIN      = 17;
static const int    STEP1_PIN     = 16;

// TB6600 #2 pins (Grove A & B)
static const int    ENABLE2_PIN   = 21;
static const int    DIR2_PIN      = 22;
static const int    STEP2_PIN     = 26;

static const int    STEP_DELAY_US = 200;   // live-drive pulse width
#define              MAX_SEGMENTS  100

struct Segment {
  int8_t           dir1, dir2;
  long             pulses1, pulses2;
  unsigned long    durationMs;
};

static Segment      segments[MAX_SEGMENTS];
static uint16_t     segmentCount   = 0;
static bool         recordingMode  = false;
static bool         playbackMode   = false;

// tracking for recording
static volatile long  stepCount1     = 0;
static volatile long  stepCount2     = 0;
static unsigned long  segStartTime   = 0;
static long           segStartCount1, segStartCount2;
static int8_t         lastDir1 = 0, lastDir2 = 0;

// button-edge storage
static bool lastLB=false, lastRB=false;
static bool lastA=false,  lastB=false;
static bool lastX=false,  lastY=false;
static bool lastBack=false, lastStart=false;

// two-step delete confirmation
static bool         pendingDelete = false;
static unsigned long pendingExpiry = 0;   // timeout for confirm

//—————————————————————————————————————————————
// Low-Level Motor Control
//—————————————————————————————————————————————

void doStep1() {
  digitalWrite(STEP1_PIN, HIGH);
  delayMicroseconds(STEP_DELAY_US);
  digitalWrite(STEP1_PIN, LOW);
  delayMicroseconds(STEP_DELAY_US);
  stepCount1++;
}

void doStep2() {
  digitalWrite(STEP2_PIN, HIGH);
  delayMicroseconds(STEP_DELAY_US);
  digitalWrite(STEP2_PIN, LOW);
  delayMicroseconds(STEP_DELAY_US);
  stepCount2++;
}

inline void enable1(bool en) { digitalWrite(ENABLE1_PIN, en ? LOW : HIGH); }
inline void enable2(bool en) { digitalWrite(ENABLE2_PIN, en ? LOW : HIGH); }

//—————————————————————————————————————————————
// Recording & Playback
//—————————————————————————————————————————————

static void clearCounts() {
  stepCount1 = stepCount2 = 0;
}

static void startSegment(int8_t d1, int8_t d2) {
  segStartTime   = millis();
  segStartCount1 = stepCount1;
  segStartCount2 = stepCount2;
}

static void recordSegment(int8_t d1, int8_t d2) {
  if (segmentCount >= MAX_SEGMENTS) return;
  unsigned long dur = millis() - segStartTime;
  long p1 = stepCount1 - segStartCount1;
  long p2 = stepCount2 - segStartCount2;
  if (d1 != 0 || d2 != 0) {
    segments[segmentCount++] = { d1, d2, p1, p2, dur };
    Serial.printf("Recorded #%u: d1=%d p1=%ld d2=%d p2=%ld t=%lums\n",
      segmentCount, d1, p1, d2, p2, dur);
  }
  startSegment(d1, d2);
}

static void saveToFlash() {
  preferences.begin("robocan", false);
  preferences.putUShort("count", segmentCount);
  preferences.putBytes("data", segments, sizeof(Segment) * segmentCount);
  preferences.end();
  Serial.printf("Saved %u segments\n", segmentCount);
}

static void loadFromFlash() {
  preferences.begin("robocan", true);
  segmentCount = preferences.getUShort("count", 0);
  if (segmentCount > MAX_SEGMENTS) segmentCount = 0;
  if (segmentCount) {
    preferences.getBytes("data", segments, sizeof(Segment) * segmentCount);
    Serial.printf("Loaded %u segments\n", segmentCount);
  } else {
    Serial.println("No saved segments");
  }
  preferences.end();
}

static void deleteSegmentsFromFlash() {
  preferences.begin("robocan", false);
  preferences.clear();  // erase all keys in this namespace
  preferences.end();
  segmentCount = 0;
  Serial.println("Deleted all saved segments");
}

// playback interleaves steps evenly over recorded duration
static void playbackSequence(bool reverse) {
  if (!segmentCount) {
    Serial.println("No recording to play");
    return;
  }
  playbackMode = true;
  Serial.println(reverse ? "--- PLAY REV ---" : "--- PLAY FWD ---");

  int16_t idx = reverse ? segmentCount - 1 : 0;
  int16_t end = reverse ? -1               : segmentCount;
  int8_t  step = reverse ? -1               : 1;

  while (idx != end && playbackMode) {
    auto &s = segments[idx];

    // set direction & enable
    digitalWrite(DIR1_PIN, s.dir1 > 0 ? HIGH : LOW);
    digitalWrite(DIR2_PIN, s.dir2 > 0 ? HIGH : LOW);
    enable1(s.dir1 != 0);
    enable2(s.dir2 != 0);

    long maxP = max(s.pulses1, s.pulses2);
    if (maxP <= 0) {
      delay(s.durationMs);
    } else {
      unsigned long totalUs  = s.durationMs * 1000UL;
      unsigned long periodUs = totalUs / maxP;
      for (long k = 0; k < maxP && playbackMode; k++) {
        unsigned long t0 = micros();
        if (k < s.pulses1) digitalWrite(STEP1_PIN, HIGH);
        if (k < s.pulses2) digitalWrite(STEP2_PIN, HIGH);
        delayMicroseconds(STEP_DELAY_US);
        if (k < s.pulses1) digitalWrite(STEP1_PIN, LOW);
        if (k < s.pulses2) digitalWrite(STEP2_PIN, LOW);

        unsigned long elapsed = micros() - t0;
        if (periodUs > elapsed) {
          delayMicroseconds(periodUs - elapsed);
        }

        xbox.onLoop();
        if (xbox.xboxNotif.btnX) {
          Serial.println("Playback aborted");
          playbackMode = false;
          break;
        }
      }
    }

    delay(50);
    idx += step;
  }

  enable1(false);
  enable2(false);
  playbackMode = false;
  Serial.println("--- PLAY COMPLETE ---");
}

//—————————————————————————————————————————————
// Display & Setup
//—————————————————————————————————————————————

static void updateDisplay() {
  // If a delete-prompt is active and not yet expired, skip normal display
  if (pendingDelete && millis() < pendingExpiry) {
    return;
  }
  // Otherwise show normal status
  pendingDelete = false;
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("REC:%s  PLAY:%s\n",
    recordingMode ? "ON " : "OFF",
    playbackMode   ? "ON " : "OFF");
  M5.Lcd.printf("Segs: %u\n", segmentCount);
}

void setup() {
  M5.begin();
  M5.Lcd.setBrightness(128);
  M5.Lcd.fillScreen(BLACK);
  Serial.begin(115200);
  delay(200);

  // configure motor pins
  pinMode(ENABLE1_PIN, OUTPUT);
  pinMode(DIR1_PIN,    OUTPUT);
  pinMode(STEP1_PIN,   OUTPUT);
  pinMode(ENABLE2_PIN, OUTPUT);
  pinMode(DIR2_PIN,    OUTPUT);
  pinMode(STEP2_PIN,   OUTPUT);

  digitalWrite(DIR1_PIN, HIGH);
  digitalWrite(DIR2_PIN, HIGH);
  enable1(false);
  enable2(false);

  xbox.begin();
  loadFromFlash();

  // pairing prompt
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Hold Xbox bind");
  M5.Lcd.println("button to pair");
}

void loop() {
  xbox.onLoop();
  if (!xbox.isConnected()) return;

  static bool shown = false;
  if (!shown) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("CONNECTED!");
    delay(800);
    shown = true;
    updateDisplay();
  }

  // read buttons
  bool curLB    = xbox.xboxNotif.btnLB;     // record toggle
  bool curRB    = xbox.xboxNotif.btnRB;     // record segment
  bool curA     = xbox.xboxNotif.btnA;      // play forward
  bool curB     = xbox.xboxNotif.btnB;      // play reverse
  bool curX     = xbox.xboxNotif.btnX;      // abort playback
  bool curY     = xbox.xboxNotif.btnY;      // delete segments
  bool curBack  = xbox.xboxNotif.btnSelect; // save flash
  bool curStart = xbox.xboxNotif.btnStart;  // load flash

  // 1) LB: toggle recording
  if (curLB && !lastLB) {
    if (!recordingMode) {
      Serial.println("> RECORD START");
      clearCounts();
      segmentCount = 0;
      recordingMode = true;
      startSegment(0, 0);
    } else {
      Serial.println("> RECORD CANCEL");
      recordingMode = false;
    }
  }

  // 2) RB: finalize current segment
  if (curRB && !lastRB && recordingMode) {
    recordSegment(lastDir1, lastDir2);
  }

  // 3) Save / Load
  if (curBack && !lastBack && !recordingMode && !playbackMode) {
    saveToFlash();
  }
  if (curStart && !lastStart && !recordingMode && !playbackMode) {
    loadFromFlash();
    updateDisplay();
  }

  // 4) Playback A/B
  if (curA && !lastA && !recordingMode && !playbackMode) {
    playbackSequence(false);
    updateDisplay();
  }
  if (curB && !lastB && !recordingMode && !playbackMode) {
    playbackSequence(true);
    updateDisplay();
  }

  // 5) Abort playback X
  if (curX && !lastX && playbackMode) {
    Serial.println("> PLAY ABORT");
    playbackMode = false;
    enable1(false);
    enable2(false);
    updateDisplay();
  }

  // 6) Y: two-step delete confirmation
  if (curY && !lastY && !recordingMode && !playbackMode) {
    if (!pendingDelete) {
      // first press: prompt operator
      pendingDelete   = true;
      pendingExpiry   = millis() + 2000;  // 2 s to confirm
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setTextSize(2);
      M5.Lcd.setTextColor(YELLOW, BLACK);
      M5.Lcd.setCursor(0, 20);
      M5.Lcd.println("Press Y again to");
      M5.Lcd.println("confirm DELETE");
    }
    else {
      // second press: delete
      deleteSegmentsFromFlash();
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setTextSize(3);
      M5.Lcd.setTextColor(RED, BLACK);
      M5.Lcd.setCursor(0, 30);
      M5.Lcd.println("DELETED ALL");
      delay(800);
      pendingDelete = false;
      updateDisplay();
    }
  }
  // timeout pendingDelete if not confirmed
  if (pendingDelete && millis() > pendingExpiry) {
    pendingDelete = false;
    updateDisplay();
  }

  // LIVE DRIVE if not playing back
  if (!playbackMode) {
    float vy = float(xbox.xboxNotif.joyLVert) /
               XboxControllerNotificationParser::maxJoy;
    float vx = float(xbox.xboxNotif.joyRHori) /
               XboxControllerNotificationParser::maxJoy;
    int8_t d1 = 0, d2 = 0;
    if      (vy > 0.6) d1 = d2 = +1;
    else if (vy < 0.4) d1 = d2 = -1;
    if      (vx > 0.6) d1 = 0;
    else if (vx < 0.4) d2 = 0;

    // recording: on direction change
    if (recordingMode && (d1 != lastDir1 || d2 != lastDir2)) {
      recordSegment(lastDir1, lastDir2);
      startSegment(d1, d2);
    }
    lastDir1 = d1; lastDir2 = d2;

    // apply live drive
    digitalWrite(DIR1_PIN, d1 > 0 ? HIGH : LOW);
    digitalWrite(DIR2_PIN, d2 > 0 ? HIGH : LOW);
    enable1(d1 != 0);
    enable2(d2 != 0);
    if (d1) doStep1();
    if (d2) doStep2();
  }

  // save button edges
  lastLB    = curLB;    lastRB    = curRB;
  lastA     = curA;     lastB     = curB;
  lastX     = curX;     lastY     = curY;
  lastBack  = curBack;  lastStart = curStart;

  // periodically refresh display
  static unsigned long t0 = 0;
  if (millis() - t0 > 200 && !pendingDelete) {
    updateDisplay();
    t0 = millis();
  }

  delay(5);
}
