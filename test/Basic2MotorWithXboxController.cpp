// src/main.cpp

#include <Arduino.h>
#include <M5Unified.h>
#include <XboxSeriesXControllerESP32_asukiaaa.hpp>

using namespace XboxSeriesXControllerESP32_asukiaaa;

//— Xbox controller host
Core xbox;

//— Motor 1 (TB6600 #1) pins
static const int ENABLE1_PIN = 5;    // ENA– (LOW=ON)
static const int DIR1_PIN    = 17;   // DIR–
static const int STEP1_PIN   = 16;   // PUL–

//— Motor 2 (TB6600 #2) pins on exposed Grove-A & Grove-B
//   ENA– → Grove-A pin 2 (GPIO21)
//   DIR– → Grove-A pin 3 (GPIO22)
//   PUL– → Grove-B pin 2 (GPIO26)
static const int ENABLE2_PIN = 21;   // Grove A S (pin 2)
static const int DIR2_PIN    = 22;   // Grove A S (pin 3)
static const int STEP2_PIN   = 26;   // Grove B S (pin 2)

//— Motor state
bool motor1Running   = false, driver1Enabled = false, dir1CW = true;
bool motor2Running   = false, driver2Enabled = false, dir2CW = true;

//— Constant pulse timing (microseconds)
const int STEP_DELAY_US = 200;

//— Previous button states (for edge-detection)
bool prevA = false, prevB = false, prevX = false;
bool prevY = false, prevLB = false, prevRB = false;

//— Emit one STEP pulse on motor 1
void doStep1() {
  digitalWrite(STEP1_PIN, HIGH);
  delayMicroseconds(STEP_DELAY_US);
  digitalWrite(STEP1_PIN, LOW);
  delayMicroseconds(STEP_DELAY_US);
}
//— Emit one STEP pulse on motor 2
void doStep2() {
  digitalWrite(STEP2_PIN, HIGH);
  delayMicroseconds(STEP_DELAY_US);
  digitalWrite(STEP2_PIN, LOW);
  delayMicroseconds(STEP_DELAY_US);
}

//— Enable/disable driver 1
void enableDriver1(bool en) {
  driver1Enabled = en;
  digitalWrite(ENABLE1_PIN, en ? LOW : HIGH);
  if (!en) motor1Running = false;
}
//— Enable/disable driver 2
void enableDriver2(bool en) {
  driver2Enabled = en;
  digitalWrite(ENABLE2_PIN, en ? LOW : HIGH);
  if (!en) motor2Running = false;
}

//— Start/stop motor 1
void startMotor1() { if (driver1Enabled) motor1Running = true; }
void stopMotor1()  { motor1Running = false; }

//— Start/stop motor 2
void startMotor2() { if (driver2Enabled) motor2Running = true; }
void stopMotor2()  { motor2Running = false; }

//— Toggle direction 1
void toggleDir1() {
  dir1CW = !dir1CW;
  digitalWrite(DIR1_PIN, dir1CW ? HIGH : LOW);
}
//— Toggle direction 2
void toggleDir2() {
  dir2CW = !dir2CW;
  digitalWrite(DIR2_PIN, dir2CW ? HIGH : LOW);
}

//— Refresh on-screen status
void updateDisplay() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 0);

  M5.Lcd.printf("M1: %s D:%s E:%s\n",
    motor1Running   ? "RUN" : "STP",
    dir1CW          ? "CW"  : "CCW",
    driver1Enabled  ? "ON"  : "OFF"
  );
  M5.Lcd.printf("M2: %s D:%s E:%s\n",
    motor2Running   ? "RUN" : "STP",
    dir2CW          ? "CW"  : "CCW",
    driver2Enabled  ? "ON"  : "OFF"
  );
}

void setup() {
  M5.begin();
  M5.Lcd.setBrightness(128);
  M5.Lcd.fillScreen(BLACK);
  Serial.begin(115200);
  delay(200);

  // Motor 1 setup
  pinMode(ENABLE1_PIN, OUTPUT);
  pinMode(DIR1_PIN,    OUTPUT);
  pinMode(STEP1_PIN,   OUTPUT);
  digitalWrite(DIR1_PIN, HIGH);  // default CW
  enableDriver1(false);

  // Motor 2 setup
  pinMode(ENABLE2_PIN, OUTPUT);
  pinMode(DIR2_PIN,    OUTPUT);
  pinMode(STEP2_PIN,   OUTPUT);
  digitalWrite(DIR2_PIN, HIGH);  // default CW
  enableDriver2(false);

  // Start BLE-HID host
  xbox.begin();

  // Prompt pairing
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Hold Xbox bind");
  M5.Lcd.println("button to pair");
}

void loop() {
  // Process BLE events
  xbox.onLoop();

  // On first connect, flash “CONNECTED!” then show status
  static bool shownConnect = false;
  if (xbox.isConnected() && !shownConnect) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("CONNECTED!");
    Serial.println("Xbox controller connected");
    delay(800);
    shownConnect = true;
    updateDisplay();
  }

  // Read Xbox buttons
  bool btnA  = xbox.xboxNotif.btnA;
  bool btnB  = xbox.xboxNotif.btnB;
  bool btnX  = xbox.xboxNotif.btnX;
  bool btnY  = xbox.xboxNotif.btnY;
  bool btnLB = xbox.xboxNotif.btnLB;
  bool btnRB = xbox.xboxNotif.btnRB;

  // Motor 1: A=start/stop, B=dir toggle, X=enable toggle
  if (btnA && !prevA) { motor1Running ? stopMotor1() : startMotor1();  updateDisplay(); }
  if (btnB && !prevB) { toggleDir1();  updateDisplay(); }
  if (btnX && !prevX) { enableDriver1(!driver1Enabled); updateDisplay(); }

  // Motor 2: Y=start/stop, RB=dir toggle, LB=enable toggle
  if (btnY  && !prevY)  { motor2Running ? stopMotor2() : startMotor2();  updateDisplay(); }
  if (btnRB && !prevRB) { toggleDir2();  updateDisplay(); }
  if (btnLB && !prevLB) { enableDriver2(!driver2Enabled); updateDisplay(); }

  // Save previous states
  prevA  = btnA;  prevB  = btnB;  prevX  = btnX;
  prevY  = btnY;  prevRB = btnRB; prevLB = btnLB;

  // Step motors if running
  if (motor1Running) doStep1();
  if (motor2Running) doStep2();

  delay(5);
}
