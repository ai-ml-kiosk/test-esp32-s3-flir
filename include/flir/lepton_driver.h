#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "flir/thermal_types.h"

namespace flir {

enum class LeptonStatus : uint8_t {
  Ok,
  Timeout,
  SyncLost,
  SpiError,
};

class LeptonDriver {
 public:
  bool begin();
  LeptonStatus readFrame(uint16_t* raw14, size_t pixelCount);
  void resync();
  uint8_t lastMissingRows() const { return lastMissingRows_; }

 private:
  static constexpr uint32_t kSpiFrequency = 8000000;
  static constexpr uint16_t kPacketBytes = 164;
  static constexpr uint8_t kPacketWords = 80;
  static constexpr uint32_t kFrameTimeoutMs = 140;
  static constexpr uint16_t kMaxDiscardPackets = 180;
  static constexpr uint8_t kMaxRenderableMissingRows = 2;

  bool readPacket(uint8_t* packet, uint16_t packetSize);
  bool readAlignedPacket(uint8_t* packet, uint8_t& packetNumber);
  void storePacketPayload(const uint8_t* packet, uint8_t packetNumber, uint16_t* raw14);
  void select();
  void deselect();

  SPIClass spi_{HSPI};
  SPISettings spiSettings_{kSpiFrequency, MSBFIRST, SPI_MODE3};
  uint32_t syncLossCount_ = 0;
  uint8_t lastMissingRows_ = 0;
};

}  // namespace flir
