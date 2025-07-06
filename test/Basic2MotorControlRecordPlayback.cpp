// src/main.cpp

#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <XboxSeriesXControllerESP32_asukiaaa.hpp>

using namespace XboxSeriesXControllerESP32_asukiaaa;

//—————————————————————————————————————————————
// Configuration & Globals
//—————————————————————————————————————————————

Preferences preferences;
Core        xbox;

// Motor 1 pins
static const int ENABLE1_PIN = 5;
static const int DIR1_PIN    = 17;
static const int STEP1_PIN   = 16;

// Motor 2 pins (Grove A & B)
static const int ENABLE2_PIN = 21;
static const int DIR2_PIN    = 22;
static const int STEP2_PIN   = 26;

static const int STEP_DELAY_US   = 200;
#define        MAX_SEGMENTS      100

struct Segment {
  int8_t           dir1;      // +1 forward, -1 backward
  int8_t           dir2;
  long             pulses1;
  long             pulses2;
  unsigned long    duration;
};

Segment segments[MAX_SEGMENTS];
uint16_t movementCount = 0;

// recording/playback flags
bool     recordingMode = false;
bool     playbackMode  = false;

// pulse counters
volatile long stepCount1 = 0, stepCount2 = 0;

// for recording timing
unsigned long segmentStart = 0;
long          startCount1, startCount2;
int8_t        lastDir1 = 0, lastDir2 = 0;

// button-edge storage
bool lastLB=false, lastRB=false, lastA=false, lastB=false;
bool lastX=false, lastBack=false, lastStart=false;

//—————————————————————————————————————————————
// Low-level motor control
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
void enable1(bool en) { digitalWrite(ENABLE1_PIN, en?LOW:HIGH); }
void enable2(bool en) { digitalWrite(ENABLE2_PIN, en?LOW:HIGH); }

//—————————————————————————————————————————————
// Recording logic
//—————————————————————————————————————————————

void clearCounts() {
  stepCount1 = stepCount2 = 0;
}

void startSegment(int8_t d1, int8_t d2) {
  segmentStart  = millis();
  startCount1   = stepCount1;
  startCount2   = stepCount2;
}

void recordSegment(int8_t d1, int8_t d2) {
  if (movementCount >= MAX_SEGMENTS) return;
  unsigned long dur = millis() - segmentStart;
  long p1 = stepCount1 - startCount1;
  long p2 = stepCount2 - startCount2;
  if (d1==0 && d2==0) return;
  segments[movementCount++] = { d1, d2, p1, p2, dur };
  Serial.printf("Seg %u: d1=%d p1=%ld, d2=%d p2=%ld, t=%lums\n",
                movementCount, d1,p1, d2,p2, dur);
  // prepare for next
  startSegment(d1,d2);
}

//—————————————————————————————————————————————
// Flash storage
//—————————————————————————————————————————————

void saveToFlash() {
  preferences.begin("robocan", false);
  preferences.putUShort("count", movementCount);
  preferences.putBytes("data", segments, sizeof(Segment)*movementCount);
  preferences.end();
  Serial.printf("Saved %u segments\n", movementCount);
}

void loadFromFlash() {
  preferences.begin("robocan", true);
  movementCount = preferences.getUShort("count", 0);
  if (movementCount > MAX_SEGMENTS) movementCount = 0;
  if (movementCount) {
    preferences.getBytes("data", segments, sizeof(Segment)*movementCount);
    Serial.printf("Loaded %u segments\n", movementCount);
  } else {
    Serial.println("No saved segments");
  }
  preferences.end();
}

//—————————————————————————————————————————————
// Playback
//—————————————————————————————————————————————

void playbackSequence(bool reverse) {
  if (!movementCount) {
    Serial.println("No recording");
    return;
  }
  playbackMode = true;
  Serial.println(reverse ? "--- PLAY REV ---" : "--- PLAY FWD ---");
  int16_t idx = reverse ? movementCount-1 : 0;
  int16_t end = reverse ? -1 : movementCount;
  int8_t  step= reverse ? -1 : 1;

  while (idx != end && playbackMode) {
    auto &s = segments[idx];
    // set dir & enable
    digitalWrite(DIR1_PIN, s.dir1>0?HIGH:LOW);
    digitalWrite(DIR2_PIN, s.dir2>0?HIGH:LOW);
    enable1(s.dir1!=0);
    enable2(s.dir2!=0);
    // interleave steps
    long maxP = max(s.pulses1, s.pulses2);
    for (long k=0; k<maxP && playbackMode; k++) {
      if (k< s.pulses1) doStep1();
      if (k< s.pulses2) doStep2();
      xbox.onLoop();
      if (xbox.xboxNotif.btnX) {
        Serial.println("Playback ABORT");
        playbackMode = false;
      }
    }
    delay(50);
    idx += step;
  }
  enable1(false);
  enable2(false);
  playbackMode = false;
  Serial.println("--- PLAY DONE ---");
}

//—————————————————————————————————————————————
// Display & Setup
//—————————————————————————————————————————————

void updateDisplay() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("REC:%s PLAY:%s\n",
    recordingMode?"ON":"OFF",
    playbackMode?"ON":"OFF");
  M5.Lcd.printf("Segs: %u\n", movementCount);
}

void setup() {
  M5.begin();
  M5.Lcd.setBrightness(128);
  M5.Lcd.fillScreen(BLACK);
  Serial.begin(115200);
  delay(200);

  // pins
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

  static bool splash=false;
  if (!splash) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setCursor(0,0);
    M5.Lcd.println("CONNECTED!");
    Serial.println("Paired");
    delay(800);
    splash=true;
    updateDisplay();
  }

  // read buttons
  bool curLB    = xbox.xboxNotif.btnLB;
  bool curRB    = xbox.xboxNotif.btnRB;
  bool curA     = xbox.xboxNotif.btnA;
  bool curB     = xbox.xboxNotif.btnB;
  bool curX     = xbox.xboxNotif.btnX;
  bool curBack  = xbox.xboxNotif.btnSelect;
  bool curStart = xbox.xboxNotif.btnStart;

  // 1) LB: toggle record
  if (curLB && !lastLB) {
    if (!recordingMode) {
      Serial.println("> REC START");
      clearCounts();
      movementCount=0;
      recordingMode=true;
      // begin first segment
      lastDir1=lastDir2=0;
      startSegment(0,0);
    } else {
      Serial.println("> REC CANCEL");
      recordingMode=false;
    }
  }

  // 2) RB: manual segment end
  if (curRB && !lastRB && recordingMode) {
    recordSegment(lastDir1, lastDir2);
  }

  // 3) Save/Load
  if (curBack && !lastBack && !recordingMode && !playbackMode) {
    saveToFlash();
  }
  if (curStart && !lastStart && !recordingMode && !playbackMode) {
    loadFromFlash();
    updateDisplay();
  }

  // 4) Playback forward/reverse
  if (curA && !lastA && !recordingMode && !playbackMode) {
    playbackSequence(false);
    updateDisplay();
  }
  if (curB && !lastB && !recordingMode && !playbackMode) {
    playbackSequence(true);
    updateDisplay();
  }

  // 5) Abort playback
  if (curX && !lastX && playbackMode) {
    Serial.println("> PLAY ABORT");
    playbackMode=false;
    enable1(false);
    enable2(false);
    updateDisplay();
  }

  // read sticks & live drive (always when not playing back)
  if (!playbackMode) {
    float vy = float(xbox.xboxNotif.joyLVert) / XboxControllerNotificationParser::maxJoy;
    float vx = float(xbox.xboxNotif.joyRHori) / XboxControllerNotificationParser::maxJoy;
    int8_t d1=0, d2=0;
    if      (vy>0.6) d1=d2=+1;
    else if (vy<0.4) d1=d2=-1;
    // steering
    if      (vx>0.6) d1=0;
    else if (vx<0.4) d2=0;

    // recording: on direction change
    if (recordingMode && (d1!=lastDir1 || d2!=lastDir2)) {
      recordSegment(lastDir1, lastDir2);
      startSegment(d1,d2);
    }
    lastDir1=d1; lastDir2=d2;

    // apply to motors
    digitalWrite(DIR1_PIN, d1>0?HIGH:LOW);
    digitalWrite(DIR2_PIN, d2>0?HIGH:LOW);
    enable1(d1!=0);
    enable2(d2!=0);
    if (d1) doStep1();
    if (d2) doStep2();
  }

  // save edges
  lastLB    = curLB;
  lastRB    = curRB;
  lastA     = curA;
  lastB     = curB;
  lastX     = curX;
  lastBack  = curBack;
  lastStart = curStart;

  // refresh display occasionally
  static unsigned long t0=0;
  if (millis()-t0>200) { updateDisplay(); t0=millis(); }
  delay(5);
}
