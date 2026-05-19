#include "flir/lepton_driver.h"

#include "pin_config.h"

namespace flir {

bool LeptonDriver::begin() {
  pinMode(Pins::FLIR_SPI_CS, OUTPUT);
  deselect();

  if (Pins::FLIR_POWER_ENABLE >= 0) {
    pinMode(Pins::FLIR_POWER_ENABLE, OUTPUT);
    digitalWrite(Pins::FLIR_POWER_ENABLE, HIGH);
  }
  if (Pins::FLIR_RESET >= 0) {
    pinMode(Pins::FLIR_RESET, OUTPUT);
    digitalWrite(Pins::FLIR_RESET, HIGH);
  }

  spi_.begin(Pins::FLIR_SPI_SCLK, Pins::FLIR_SPI_MISO, Pins::FLIR_SPI_MOSI, Pins::FLIR_SPI_CS);
  delay(250);
  char line[128] = {};
  snprintf(line,
           sizeof(line),
           "Lepton VoSPI initialized: dedicated SPI, mode=3, freq=%lu Hz, SCLK=%d MISO=%d MOSI=%d CS=%d\n",
           static_cast<unsigned long>(kSpiFrequency),
           Pins::FLIR_SPI_SCLK,
           Pins::FLIR_SPI_MISO,
           Pins::FLIR_SPI_MOSI,
           Pins::FLIR_SPI_CS);
  Serial.print(line);
  Serial0.print(line);
  return true;
}

LeptonStatus LeptonDriver::readFrame(uint16_t* raw14, size_t pixelCount) {
  lastMissingRows_ = 0;
  if (raw14 == nullptr || pixelCount < kLeptonPixels) {
    Serial.println("Lepton read error: raw frame buffer is too small");
    return LeptonStatus::SpiError;
  }

  uint8_t packet[kPacketBytes] = {};
  const uint32_t started = millis();

  uint8_t packetNumber = 0;
  while (true) {
    if (millis() - started > kFrameTimeoutMs) {
      Serial.println("Lepton read error: frame timeout waiting for packet 0");
      resync();
      return LeptonStatus::Timeout;
    }
    if (!readAlignedPacket(packet, packetNumber)) {
      Serial.println("Lepton read error: lost VoSPI packet sync");
      resync();
      return LeptonStatus::SyncLost;
    }
    if (packetNumber == 0) {
      storePacketPayload(packet, 0, raw14);
      break;
    }
  }

  uint8_t nextPacket = 1;
  uint8_t missingPackets = 0;
  while (nextPacket < kLeptonHeight) {
    if (millis() - started > kFrameTimeoutMs) {
      Serial.println("Lepton read error: frame timeout during packet sequence");
      resync();
      return LeptonStatus::Timeout;
    }

    if (!readAlignedPacket(packet, packetNumber)) {
      Serial.println("Lepton read error: lost VoSPI packet sync");
      resync();
      return LeptonStatus::SyncLost;
    }

    if (packetNumber == 0) {
      storePacketPayload(packet, 0, raw14);
      nextPacket = 1;
      missingPackets = 0;
      continue;
    }
    if (packetNumber != nextPacket) {
      if (packetNumber > nextPacket) {
        missingPackets += packetNumber - nextPacket;
        nextPacket = packetNumber;
      } else {
        continue;
      }
    }

    storePacketPayload(packet, packetNumber, raw14);
    ++nextPacket;
  }

  lastMissingRows_ = missingPackets;
  if (missingPackets > 0) {
    char line[80] = {};
    snprintf(line, sizeof(line), "Lepton frame warning: filled with %u stale rows\n", missingPackets);
    Serial.print(line);
    Serial0.print(line);
  }
  if (missingPackets > kMaxRenderableMissingRows) {
    resync();
    return LeptonStatus::SyncLost;
  }
  return LeptonStatus::Ok;
}

void LeptonDriver::resync() {
  ++syncLossCount_;
  deselect();
  delay(185);
  if (syncLossCount_ <= 5 || (syncLossCount_ % 10) == 0) {
    char line[72] = {};
    snprintf(line,
             sizeof(line),
             "Lepton VoSPI resync requested, count=%lu\n",
             static_cast<unsigned long>(syncLossCount_));
    Serial.print(line);
    Serial0.print(line);
  }
}

bool LeptonDriver::readPacket(uint8_t* packet, uint16_t packetSize) {
  if (packet == nullptr || packetSize != kPacketBytes) {
    return false;
  }

  static uint8_t txZeros[kPacketBytes] = {};
  spi_.beginTransaction(spiSettings_);
  select();
  spi_.transferBytes(txZeros, packet, packetSize);
  deselect();
  spi_.endTransaction();
  return true;
}

bool LeptonDriver::readAlignedPacket(uint8_t* packet, uint8_t& packetNumber) {
  for (uint16_t attempts = 0; attempts < kMaxDiscardPackets; ++attempts) {
    if (!readPacket(packet, kPacketBytes)) {
      return false;
    }

    if ((packet[0] & 0x0F) == 0x0F) {
      delayMicroseconds(10);
      continue;
    }

    packetNumber = packet[1] & 0x3F;
    if (packetNumber < kLeptonHeight) {
      return true;
    }
  }
  return false;
}

void LeptonDriver::storePacketPayload(const uint8_t* packet, uint8_t packetNumber, uint16_t* raw14) {
  const size_t rowOffset = static_cast<size_t>(packetNumber) * kLeptonWidth;
  for (uint8_t x = 0; x < kPacketWords; ++x) {
    const size_t packetOffset = 4 + static_cast<size_t>(x) * 2;
    const uint8_t highByte = packet[packetOffset];
    const uint8_t lowByte = packet[packetOffset + 1];

    // Lepton VoSPI sends each pixel as two sequential bytes, MSB first.
    // Shift the high byte into bits 15:8, then OR in the low byte.
    const uint16_t rawValue = (static_cast<uint16_t>(highByte) << 8) | lowByte;

    // In radiometric TLinear mode the payload is a full 16-bit centikelvin
    // temperature. Do not apply a 14-bit mask here: room temperature is around
    // 29315 centikelvin, and masking it would corrupt it into the -140C range.
    raw14[rowOffset + x] = rawValue;
  }
}

void LeptonDriver::select() {
  digitalWrite(Pins::FLIR_SPI_CS, LOW);
}

void LeptonDriver::deselect() {
  digitalWrite(Pins::FLIR_SPI_CS, HIGH);
}

}  // namespace flir
