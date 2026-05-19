#pragma once

#include <Arduino.h>
#include <SD.h>

#include "flir/thermal_types.h"

namespace flir {

class CaptureStorage {
 public:
  bool begin();
  bool isReady() const;
  bool saveCapture(const uint16_t* raw14,
                   size_t rawPixelCount,
                   const uint8_t* bitmap8,
                   uint16_t bitmapWidth,
                   uint16_t bitmapHeight);

 private:
  bool writeRawRadiometric(const char* path, const uint16_t* raw14, size_t pixelCount) const;
  bool writeGrayscaleBmp(const char* path,
                         const uint8_t* bitmap8,
                         uint16_t width,
                         uint16_t height) const;
  bool ensureCaptureDir() const;
  uint32_t nextCaptureId();

  bool ready_ = false;
  uint32_t captureId_ = 0;
};

}  // namespace flir
