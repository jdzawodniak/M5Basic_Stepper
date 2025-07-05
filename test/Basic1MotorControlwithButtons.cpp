#include <Arduino.h>
#include <M5Unified.h>

#define enablePin 5
#define directionPin 17
#define stepPin 16
#define stepsPerRevolution 1600
#define Revolutions 5
#define speed 150

bool motorRunning = false;
bool direction = false;  // false = CW, true = CCW
bool driverEnabled = true;

void updateDisplay() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("Motor: %s\n", motorRunning ? "RUNNING" : "STOPPED");
  M5.Lcd.printf("Direction: %s\n", direction ? "CCW" : "CW");
  M5.Lcd.printf("Driver: %s\n", driverEnabled ? "ENABLED" : "DISABLED");
}

void setup() {
  M5.begin();
  M5.Speaker.end();  // Mute speaker
  M5.Lcd.setBrightness(255);
  pinMode(stepPin, OUTPUT);
  pinMode(directionPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, LOW); // LOW = enabled
  Serial.begin(115200);
  updateDisplay();
}

void loop() {
  M5.update();

  // Toggle motor on/off with Button A
  if (M5.BtnA.wasPressed() && driverEnabled) {
    motorRunning = !motorRunning;
    Serial.println(motorRunning ? "Motor STARTED" : "Motor STOPPED");
    updateDisplay();
  }

  // Toggle direction with Button B
  if (M5.BtnB.wasPressed()) {
    direction = !direction;
    Serial.print("Direction changed to: ");
    Serial.println(direction ? "CCW" : "CW");
    updateDisplay();
  }

  // Toggle driver enable/disable with Button C
  if (M5.BtnC.wasPressed()) {
    driverEnabled = !driverEnabled;
    digitalWrite(enablePin, driverEnabled ? LOW : HIGH);
    Serial.printf("Driver %s via Button C\n", driverEnabled ? "ENABLED" : "DISABLED");

    if (!driverEnabled) {
      motorRunning = false;
    }

    updateDisplay();
  }

  // Run motor if enabled
  if (motorRunning && driverEnabled) {
    digitalWrite(directionPin, direction);
    for (int i = 0; i < Revolutions * stepsPerRevolution; i++) {
      if (!motorRunning || !driverEnabled) break;
      digitalWrite(stepPin, HIGH);
      delayMicroseconds(speed);
      digitalWrite(stepPin, LOW);
      delayMicroseconds(speed);
      M5.update();

      if (M5.BtnA.wasPressed()) {
        motorRunning = false;
        Serial.println("Motor STOPPED mid-run");
        updateDisplay();
        break;
      }

      if (M5.BtnB.wasPressed()) {
        direction = !direction;
        digitalWrite(directionPin, direction);
        Serial.print("Direction changed mid-run to: ");
        Serial.println(direction ? "CCW" : "CW");
        updateDisplay();
      }

      if (M5.BtnC.wasPressed()) {
        driverEnabled = !driverEnabled;
        digitalWrite(enablePin, driverEnabled ? LOW : HIGH);
        Serial.printf("Driver %s mid-run\n", driverEnabled ? "ENABLED" : "DISABLED");
        if (!driverEnabled) {
          motorRunning = false;
        }
        updateDisplay();
        break;
      }
    }
  }

  delay(10);
}
