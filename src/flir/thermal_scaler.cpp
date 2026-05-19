#include "flir/thermal_scaler.h"

namespace flir {

uint16_t Thermalscaler::rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint8_t Thermalscaler::lerp8(uint8_t a, uint8_t b, uint8_t amount) {
  return a + (((static_cast<int16_t>(b) - a) * amount) >> 8);
}

uint16_t Thermalscaler::blend565(uint16_t a, uint16_t b, uint8_t amount) {
  const uint8_t ar = ((a >> 11) & 0x1F) << 3;
  const uint8_t ag = ((a >> 5) & 0x3F) << 2;
  const uint8_t ab = (a & 0x1F) << 3;
  const uint8_t br = ((b >> 11) & 0x1F) << 3;
  const uint8_t bg = ((b >> 5) & 0x3F) << 2;
  const uint8_t bb = (b & 0x1F) << 3;
  return rgb565(lerp8(ar, br, amount), lerp8(ag, bg, amount), lerp8(ab, bb, amount));
}

const uint16_t* Thermalscaler::ironbowPalette() {
  static constexpr uint16_t kIronbowStops[] = {
      0x0000,  // black
      0x180F,  // deep violet
      0x481F,  // purple
      0xF800,  // red
      0xFD20,  // orange
      0xFFE0,  // yellow
      0xFFFF,  // white
  };
  static uint16_t palette[kPaletteSize];
  static bool ready = false;
  if (ready) {
    return palette;
  }

  static constexpr uint8_t kSegmentCount = sizeof(kIronbowStops) / sizeof(kIronbowStops[0]) - 1;
  for (uint16_t value = 0; value < kPaletteSize; ++value) {
    const uint16_t scaled = value * kSegmentCount;
    const uint8_t index = min<uint8_t>(kSegmentCount - 1, scaled / 255);
    const uint8_t amount = scaled - (index * 255);
    palette[value] = blend565(kIronbowStops[index], kIronbowStops[index + 1], amount);
  }
  ready = true;
  return palette;
}

bool Thermalscaler::renderIronbow(const uint8_t* src80x60, uint16_t* rgb565Out) const {
  if (src80x60 == nullptr || rgb565Out == nullptr) {
    Serial.println("Thermalscaler error: null source or destination buffer");
    return false;
  }

  const uint16_t* palette = ironbowPalette();
  static uint8_t x0Table[kOutputWidth];
  static uint8_t x1Table[kOutputWidth];
  static uint8_t xWeightTable[kOutputWidth];
  static uint8_t y0Table[kOutputHeight];
  static uint8_t y1Table[kOutputHeight];
  static uint8_t yWeightTable[kOutputHeight];
  static bool tablesReady = false;
  if (!tablesReady) {
    const uint32_t xRatio = ((static_cast<uint32_t>(kInputWidth - 1)) << 16) / (kOutputWidth - 1);
    const uint32_t yRatio = ((static_cast<uint32_t>(kInputHeight - 1)) << 16) / (kOutputHeight - 1);
    for (uint16_t x = 0; x < kOutputWidth; ++x) {
      const uint32_t srcX = static_cast<uint32_t>(x) * xRatio;
      const uint8_t x0 = srcX >> 16;
      x0Table[x] = x0;
      x1Table[x] = min<uint8_t>(x0 + 1, kInputWidth - 1);
      xWeightTable[x] = (srcX & 0xFFFF) >> 8;
    }
    for (uint16_t y = 0; y < kOutputHeight; ++y) {
      const uint32_t srcY = static_cast<uint32_t>(y) * yRatio;
      const uint8_t y0 = srcY >> 16;
      y0Table[y] = y0;
      y1Table[y] = min<uint8_t>(y0 + 1, kInputHeight - 1);
      yWeightTable[y] = (srcY & 0xFFFF) >> 8;
    }
    tablesReady = true;
  }

  for (uint16_t y = 0; y < kOutputHeight; ++y) {
    const uint8_t y0 = y0Table[y];
    const uint8_t y1 = y1Table[y];
    const uint8_t yWeight = yWeightTable[y];

    for (uint16_t x = 0; x < kOutputWidth; ++x) {
      const uint8_t x0 = x0Table[x];
      const uint8_t x1 = x1Table[x];
      const uint8_t xWeight = xWeightTable[x];

      // 16.16 fixed-point source coordinates choose the four neighboring
      // Lepton pixels. The low 16 bits become 8-bit blend weights, avoiding
      // floating-point math and per-pixel coordinate division in the ESP32-S3
      // inner render loop.
      const uint8_t p00 = src80x60[static_cast<size_t>(y0) * kInputWidth + x0];
      const uint8_t p10 = src80x60[static_cast<size_t>(y0) * kInputWidth + x1];
      const uint8_t p01 = src80x60[static_cast<size_t>(y1) * kInputWidth + x0];
      const uint8_t p11 = src80x60[static_cast<size_t>(y1) * kInputWidth + x1];
      const uint8_t top = lerp8(p00, p10, xWeight);
      const uint8_t bottom = lerp8(p01, p11, xWeight);
      const uint8_t smoothed = lerp8(top, bottom, yWeight);

      rgb565Out[static_cast<size_t>(y) * kOutputWidth + x] = palette[smoothed];
    }
  }
  return true;
}

bool Thermalscaler::renderIronbowToDisplay(const uint8_t* src80x60,
                                           uint16_t* rgb565Out,
                                           lgfx::LGFX_Device& display) const {
  if (!renderIronbow(src80x60, rgb565Out)) {
    return false;
  }
  display.pushImage(kThermalViewX, kThermalViewY, kOutputWidth, kOutputHeight, rgb565Out);
  return true;
}

bool Thermalscaler::renderIronbowAt9Hz(const uint8_t* src80x60,
                                       uint16_t* rgb565Out,
                                       lgfx::LGFX_Device& display,
                                       uint32_t nowMs) {
  if (lastRenderMs_ != 0 && nowMs - lastRenderMs_ < kFrameIntervalMs) {
    return false;
  }
  if (!renderIronbowToDisplay(src80x60, rgb565Out, display)) {
    return false;
  }
  lastRenderMs_ = nowMs;
  return true;
}

}  // namespace flir
