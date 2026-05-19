#include "flir/capture_storage.h"

#include <SPI.h>

#include "pin_config.h"

namespace flir {
namespace {

void write16(File& file, uint16_t value) {
  file.write(value & 0xFF);
  file.write((value >> 8) & 0xFF);
}

void write32(File& file, uint32_t value) {
  write16(file, value & 0xFFFF);
  write16(file, value >> 16);
}

}  // namespace

bool CaptureStorage::begin() {
  pinMode(Pins::LCD_CS, OUTPUT);
  pinMode(Pins::TOUCH_CS, OUTPUT);
  pinMode(Pins::SD_CS, OUTPUT);
  digitalWrite(Pins::LCD_CS, HIGH);
  digitalWrite(Pins::TOUCH_CS, HIGH);
  digitalWrite(Pins::SD_CS, HIGH);

  SPI.begin(Pins::UI_SPI_SCLK, Pins::UI_SPI_MISO, Pins::UI_SPI_MOSI, Pins::SD_CS);
  ready_ = SD.begin(Pins::SD_CS, SPI, 20000000);
  if (!ready_) {
    Serial.println("SD error: failed to mount card on shared UI SPI bus");
    return false;
  }
  if (!ensureCaptureDir()) {
    Serial.println("SD error: failed to create /flir capture directory");
    ready_ = false;
    return false;
  }
  Serial.println("SD card mounted for FLIR captures");
  return true;
}

bool CaptureStorage::isReady() const {
  return ready_;
}

bool CaptureStorage::saveCapture(const uint16_t* raw14,
                                 size_t rawPixelCount,
                                 const uint8_t* bitmap8,
                                 uint16_t bitmapWidth,
                                 uint16_t bitmapHeight) {
  if (!ready_) {
    Serial.println("Capture error: SD card is not mounted");
    return false;
  }
  if (raw14 == nullptr || bitmap8 == nullptr || rawPixelCount == 0 || bitmapWidth == 0 || bitmapHeight == 0) {
    Serial.println("Capture error: invalid image buffers");
    return false;
  }

  const uint32_t id = nextCaptureId();
  char rawPath[40] = {};
  char bmpPath[40] = {};
  snprintf(rawPath, sizeof(rawPath), "/flir/frame_%05lu.raw", static_cast<unsigned long>(id));
  snprintf(bmpPath, sizeof(bmpPath), "/flir/frame_%05lu.bmp", static_cast<unsigned long>(id));

  const bool rawOk = writeRawRadiometric(rawPath, raw14, rawPixelCount);
  const bool bmpOk = writeGrayscaleBmp(bmpPath, bitmap8, bitmapWidth, bitmapHeight);
  if (rawOk && bmpOk) {
    Serial.printf("Capture saved: %s and %s\n", rawPath, bmpPath);
    return true;
  }

  Serial.printf("Capture error: failed writing %s or %s\n", rawPath, bmpPath);
  return false;
}

bool CaptureStorage::writeRawRadiometric(const char* path, const uint16_t* raw14, size_t pixelCount) const {
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.printf("SD error: unable to open %s for raw write\n", path);
    return false;
  }

  file.write(reinterpret_cast<const uint8_t*>(raw14), pixelCount * sizeof(uint16_t));
  const bool ok = file.getWriteError() == 0;
  file.close();
  if (!ok) {
    Serial.printf("SD error: raw write failed for %s\n", path);
  }
  return ok;
}

bool CaptureStorage::writeGrayscaleBmp(const char* path,
                                       const uint8_t* bitmap8,
                                       uint16_t width,
                                       uint16_t height) const {
  const uint32_t rowStride = (static_cast<uint32_t>(width) + 3U) & ~3U;
  const uint32_t paletteBytes = 256UL * 4UL;
  const uint32_t pixelBytes = rowStride * height;
  const uint32_t dataOffset = 14UL + 40UL + paletteBytes;
  const uint32_t fileSize = dataOffset + pixelBytes;

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.printf("SD error: unable to open %s for BMP write\n", path);
    return false;
  }

  file.write('B');
  file.write('M');
  write32(file, fileSize);
  write16(file, 0);
  write16(file, 0);
  write32(file, dataOffset);

  write32(file, 40);
  write32(file, width);
  write32(file, height);
  write16(file, 1);
  write16(file, 8);
  write32(file, 0);
  write32(file, pixelBytes);
  write32(file, 2835);
  write32(file, 2835);
  write32(file, 256);
  write32(file, 0);

  for (uint16_t i = 0; i < 256; ++i) {
    file.write(static_cast<uint8_t>(i));
    file.write(static_cast<uint8_t>(i));
    file.write(static_cast<uint8_t>(i));
    file.write(static_cast<uint8_t>(0));
  }

  uint8_t padding[3] = {};
  for (int32_t y = height - 1; y >= 0; --y) {
    file.write(bitmap8 + (static_cast<size_t>(y) * width), width);
    file.write(padding, rowStride - width);
  }

  const bool ok = file.getWriteError() == 0;
  file.close();
  if (!ok) {
    Serial.printf("SD error: BMP write failed for %s\n", path);
  }
  return ok;
}

bool CaptureStorage::ensureCaptureDir() const {
  if (SD.exists("/flir")) {
    return true;
  }
  return SD.mkdir("/flir");
}

uint32_t CaptureStorage::nextCaptureId() {
  return ++captureId_;
}

}  // namespace flir
