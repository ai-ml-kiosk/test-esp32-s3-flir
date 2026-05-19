#include <Arduino.h>

#include "display_driver.h"
#include "pin_config.h"
#include "thermal_ui.h"

Display display;
ThermalUi thermalUi;

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(Pins::SD_CS, OUTPUT);
  digitalWrite(Pins::SD_CS, HIGH);

  display.init();
  display.setRotation(1);
  display.setBrightness(210);

  thermalUi.begin(display);
}

void loop() {
  uint16_t x = 0;
  uint16_t y = 0;
  if (display.getTouch(&x, &y)) {
    thermalUi.handleTouch(x, y);
  }

  thermalUi.update(display);
  delay(100);
}
