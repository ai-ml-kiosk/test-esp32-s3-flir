#pragma once

#include <Arduino.h>

namespace flir {

static constexpr uint16_t kLeptonWidth = 80;
static constexpr uint16_t kLeptonHeight = 60;
static constexpr uint16_t kDisplayWidth = 240;
static constexpr uint16_t kDisplayHeight = 320;
static constexpr uint16_t kTopOverlayHeight = 36;
static constexpr uint16_t kControlPanelY = 246;
static constexpr uint16_t kControlPanelHeight = kDisplayHeight - kControlPanelY;
static constexpr uint16_t kThermalViewWidth = 240;
static constexpr uint16_t kThermalViewHeight = kControlPanelY - kTopOverlayHeight;
static constexpr uint16_t kThermalViewX = 0;
static constexpr uint16_t kThermalViewY = kTopOverlayHeight;
static constexpr size_t kLeptonPixels = static_cast<size_t>(kLeptonWidth) * kLeptonHeight;
static constexpr size_t kDisplayPixels = static_cast<size_t>(kDisplayWidth) * kDisplayHeight;
static constexpr size_t kThermalViewPixels = static_cast<size_t>(kThermalViewWidth) * kThermalViewHeight;

struct SpotTemperatures {
  float minC = 0.0f;
  float maxC = 0.0f;
  float centerC = 0.0f;
  uint16_t minRaw = 0;
  uint16_t maxRaw = 0;
  uint16_t centerRaw = 0;
  uint16_t minX = 0;
  uint16_t minY = 0;
  uint16_t maxX = 0;
  uint16_t maxY = 0;
};

enum class PaletteMode : uint8_t {
  Ironbow,
  WhiteHot,
  BlackHot,
  Histogram,
};

}  // namespace flir
