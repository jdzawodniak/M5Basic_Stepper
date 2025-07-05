// src/main.cpp

#include <Arduino.h>
#include <M5Unified.h>
#include <XboxSeriesXControllerESP32_asukiaaa.hpp>

using namespace XboxSeriesXControllerESP32_asukiaaa;

//— Xbox controller host
Core xbox;

//— TB6600 stepper pins
static const int ENABLE_PIN = 5;    // ENA- (active LOW)
static const int DIR_PIN    = 17;   // DIR-
static const int STEP_PIN   = 16;   // PUL-

//— Motor state
bool motorRunning  = false;
bool driverEnabled = false;
bool directionCW   = true;
int  stepDelayUs   = 200;           // constant pulse delay

//— Previous button states
bool prevA = false;
bool prevB = false;
bool prevX = false;

//— Generate one step pulse
void doStep() {
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(stepDelayUs);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(stepDelayUs);
}

//— Enable or disable the driver
void enableDriver(bool en) {
  driverEnabled = en;
  digitalWrite(ENABLE_PIN, en ? LOW : HIGH);
  if (!en) motorRunning = false;
}

//— Start or stop the motor
void startMotor() {
  if (!driverEnabled) return;
  motorRunning = true;
}
void stopMotor() {
  motorRunning = false;
}

//— Toggle rotation direction
void toggleDirection() {
  directionCW = !directionCW;
  digitalWrite(DIR_PIN, directionCW ? HIGH : LOW);
}

//— Refresh the on-screen status
void updateDisplay() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("Motor: %s\n", motorRunning  ? "RUNNING"  : "STOPPED");
  M5.Lcd.printf("Driver: %s\n", driverEnabled ? "ENABLED" : "DISABLED");
  M5.Lcd.printf("Dir: %s\n", directionCW    ? "CW"       : "CCW");
}

void setup() {
  // Initialize M5Stack
  M5.begin();
  M5.Lcd.setBrightness(128);
  M5.Lcd.fillScreen(BLACK);
  Serial.begin(115200);
  delay(200);

  // Configure TB6600 pins
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(DIR_PIN,    OUTPUT);
  pinMode(STEP_PIN,   OUTPUT);
  digitalWrite(DIR_PIN, HIGH);    // default CW
  enableDriver(false);            // start disabled

  // Start the Xbox BLE-HID host
  xbox.begin();

  // Prompt pairing on screen
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Hold Xbox bind");
  M5.Lcd.println("button to pair");
}

void loop() {
  // Handle BLE events
  xbox.onLoop();

  // Show “CONNECTED!” once
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

  // Read buttons
  bool btnA = xbox.xboxNotif.btnA;
  bool btnB = xbox.xboxNotif.btnB;
  bool btnX = xbox.xboxNotif.btnX;

  // A pressed → start/stop
  if (btnA && !prevA) {
    motorRunning ? stopMotor() : startMotor();
    updateDisplay();
  }

  // B pressed → toggle direction
  if (btnB && !prevB) {
    toggleDirection();
    updateDisplay();
  }

  // X pressed → enable/disable driver
  if (btnX && !prevX) {
    enableDriver(!driverEnabled);
    updateDisplay();
  }

  // Save states for edge detection
  prevA = btnA;
  prevB = btnB;
  prevX = btnX;

  // If running, emit continuous steps
  if (motorRunning) {
    doStep();
  }

  delay(5);
}
