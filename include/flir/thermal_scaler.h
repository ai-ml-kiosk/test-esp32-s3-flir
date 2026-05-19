#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>

#include "flir/thermal_types.h"

namespace flir {

class Thermalscaler {
 public:
  static constexpr uint16_t kInputWidth = kLeptonWidth;
  static constexpr uint16_t kInputHeight = kLeptonHeight;
  static constexpr uint16_t kOutputWidth = kThermalViewWidth;
  static constexpr uint16_t kOutputHeight = kThermalViewHeight;
  static constexpr size_t kOutputPixels = static_cast<size_t>(kOutputWidth) * kOutputHeight;
  static constexpr uint32_t kFrameIntervalMs = 111;

  bool renderIronbow(const uint8_t* src80x60, uint16_t* rgb565Out) const;
  bool renderIronbowToDisplay(const uint8_t* src80x60, uint16_t* rgb565Out, lgfx::LGFX_Device& display) const;
  bool renderIronbowAt9Hz(const uint8_t* src80x60,
                          uint16_t* rgb565Out,
                          lgfx::LGFX_Device& display,
                          uint32_t nowMs = millis());

 private:
  static constexpr uint16_t kPaletteSize = 256;

  mutable uint32_t lastRenderMs_ = 0;

  static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
  static uint8_t lerp8(uint8_t a, uint8_t b, uint8_t amount);
  static uint16_t blend565(uint16_t a, uint16_t b, uint8_t amount);
  static const uint16_t* ironbowPalette();
};

}  // namespace flir
