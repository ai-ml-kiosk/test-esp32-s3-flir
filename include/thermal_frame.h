#pragma once

constexpr int kThermalFrameWidth = 80;
constexpr int kThermalFrameHeight = 60;

struct ThermalFrame {
  float pixels[kThermalFrameHeight][kThermalFrameWidth] = {};
  float lowTempC = 0.0f;
  float highTempC = 0.0f;
  int lowX = 0;
  int lowY = 0;
  int highX = 0;
  int highY = 0;
};
