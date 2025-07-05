# 🧠 Stepper Motor Control with M5Stack Basic and TB6600

## 📦 Components Used
- M5Stack Basic (ESP32-based microcontroller)
- TB6600 Stepper Driver
- 57J1856-430-8 Stepper Motor (NEMA 23, 3.0A/phase)
- External 12–36V DC power supply

---

## 🔌 Wiring Diagram

| TB6600 Pin | Connects To M5Stack | Notes                          |
|------------|---------------------|--------------------------------|
| PUL+       | GPIO 16             | Step signal                    |
| PUL−       | GND                 |                                |
| DIR+       | GPIO 17             | Direction signal               |
| DIR−       | GND                 |                                |
| ENA+       | Leave unconnected or tie to 5V |
| ENA−       | GPIO 2 (optional)   | LOW = enabled, HIGH = disabled |
| V+         | 12–36V DC           | External power supply (+)      |
| V−         | GND                 | External power supply (−)      |
| A+/A−      | Motor coil A        |                                |
| B+/B−      | Motor coil B        |                                |

---

## 🎛 TB6600 DIP Switch Settings

| Switch | Setting | Function              |
|--------|---------|-----------------------|
| 1      | ON      | Microstepping = 1/16  |
| 2      | OFF     |                       |
| 3      | OFF     |                       |
| 4      | ON      | Current = 3.0A        |
| 5      | OFF     |                       |
| 6      | OFF     |                       |

---

## 🧪 Test Code

```cpp
#include <Arduino.h>
#include <M5Unified.h>

#define enablePin 2
#define directionPin 17
#define stepPin 16
#define stepsPerRevolution 1600
#define Revolutions 5
#define Direction 0  // 1 = counterclockwise
#define speed 150    // microseconds between pulses

void setup() {
  pinMode(stepPin, OUTPUT);
  pinMode(directionPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, LOW); // LOW = enabled
}

void loop() {
  digitalWrite(directionPin, dir);
  for (int i = 0; i < Revolutions * stepsPerRevolution; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(speed);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(speed);
  }
  dir = !dir;  // Flip direction
  delay(1000); // Pause between reversals
}
