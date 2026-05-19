#pragma once

#include <Arduino.h>

#include "thermal_frame.h"

class ThermalSimulator {
 public:
  void nextFrame(ThermalFrame& frame);

 private:
  uint32_t frameCounter_ = 0;
};
