#include <Arduino.h>
#include <M5Unified.h>

void setup() {
  M5.begin();
  M5.Display.setBrightness(255);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.fillScreen(BLACK);
  M5.Display.drawString("Display test", 10, 10);
  M5.Display.display();  // Force flush
}

void loop() {}
