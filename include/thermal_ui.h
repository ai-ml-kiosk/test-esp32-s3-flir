#pragma once

#include <Arduino.h>

#include "display_driver.h"
#include "thermal_frame.h"
#include "thermal_simulator.h"

class ThermalUi {
 public:
  void begin(Display& display);
  void update(Display& display);
  void handleTouch(uint16_t x, uint16_t y);

 private:
  struct Button {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    const char* label;
  };

  void printPinSummary() const;
  void drawFrame(Display& display);
  void drawThermalFrame(Display& display);
  void drawThermalOverlayText(Display& display);
  void drawSidePanel(Display& display);
  void drawButton(Display& display, const Button& button, bool active = false) const;
  void drawStatusRow(Display& display, int16_t y, const char* label, const String& value) const;
  void drawMarker(Display& display, int px, int py, uint16_t color) const;
  bool contains(const Button& button, uint16_t x, uint16_t y) const;

  ThermalSimulator simulator_;
  ThermalFrame frame_;
  float fps_ = 0.0f;
  float zoom_ = 1.0f;
  int rotationDeg_ = 360;
  bool thermalOverlay_ = false;
  bool captureFlash_ = false;
  uint32_t captureFlashUntilMs_ = 0;
  uint32_t lastFrameMs_ = 0;
  uint32_t lastTouchMs_ = 0;

  Button thermalButton_{248, 72, 64, 28, "THM"};
  Button rotLeftButton_{248, 128, 30, 24, "R<"};
  Button rotRightButton_{282, 128, 30, 24, "R>"};
  Button zoomOutButton_{248, 172, 30, 24, "Z-"};
  Button zoomInButton_{282, 172, 30, 24, "Z+"};
  Button captureButton_{248, 214, 30, 20, "CAP"};
  Button exitButton_{282, 214, 30, 20, "EXT"};
};
