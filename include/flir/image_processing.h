#pragma once

#include <Arduino.h>

#include "flir/thermal_types.h"

namespace flir {

class ImageProcessor {
 public:
  bool automaticGainControl(const uint16_t* raw14,
                            size_t pixelCount,
                            uint8_t* out8,
                            uint16_t* minRaw = nullptr,
                            uint16_t* maxRaw = nullptr) const;

  bool histogramEqualizeRaw14(const uint16_t* raw14,
                              size_t pixelCount,
                              uint8_t* out8,
                              uint16_t* minRaw = nullptr,
                              uint16_t* maxRaw = nullptr) const;

  bool isFrameUsable(const uint16_t* raw14,
                     size_t pixelCount,
                     uint16_t* minRaw = nullptr,
                     uint16_t* maxRaw = nullptr) const;

  bool upscaleBilinear(const uint8_t* src,
                       uint16_t srcWidth,
                       uint16_t srcHeight,
                       uint8_t* dst,
                       uint16_t dstWidth,
                       uint16_t dstHeight) const;

  bool denoise3x3(const uint8_t* src,
                  uint8_t* dst,
                  uint16_t width,
                  uint16_t height) const;

	  bool temporalSmooth(uint8_t* frame, size_t pixelCount, uint8_t previousWeight = 3) const;

	  bool sharpen3x3(uint8_t* frame, uint16_t width, uint16_t height, uint8_t amount = 1) const;

	  void applyPalette(const uint8_t* normalized,
                    size_t pixelCount,
                    uint16_t* rgb565Out,
                    PaletteMode palette) const;

  SpotTemperatures calculateSpotTemperatures(const uint16_t* raw14,
                                             uint16_t width,
                                             uint16_t height) const;

  float rawCentikelvinToCelsius(uint16_t raw14) const;

 private:
  uint16_t ironbow(uint8_t value) const;
  uint16_t grayscale(uint8_t value, bool blackHot) const;
};

}  // namespace flir
