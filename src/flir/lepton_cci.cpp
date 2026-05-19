#include "flir/lepton_cci.h"

#include "pin_config.h"

namespace flir {

bool LeptonCci::begin() {
  Wire.begin(Pins::FLIR_CCI_SDA, Pins::FLIR_CCI_SCL);
  Wire.setClock(kBusFrequency);
  Wire.setTimeOut(50);

  Wire.beginTransmission(kLeptonAddress);
  const uint8_t error = Wire.endTransmission();
  present_ = error == 0;
  if (present_) {
    Serial.printf("Lepton CCI detected at 0x%02X on SDA=%d SCL=%d\n",
                  kLeptonAddress,
                  Pins::FLIR_CCI_SDA,
                  Pins::FLIR_CCI_SCL);
  } else {
    Serial.printf("Lepton CCI warning: no ACK at 0x%02X on SDA=%d SCL=%d, Wire error=%u\n",
                  kLeptonAddress,
                  Pins::FLIR_CCI_SDA,
                  Pins::FLIR_CCI_SCL,
                  error);
  }
  return present_;
}

bool LeptonCci::isPresent() const {
  return present_;
}

bool LeptonCci::configureImageQuality() {
  if (!present_) {
    Serial.println("Lepton CCI config skipped: CCI is not present");
    return false;
  }

  uint16_t status = 0;
  if (readStatus(&status)) {
    Serial.printf("Lepton CCI status before config: 0x%04X\n", status);
  }

  bool ok = true;
  ok = setAgcEnabled(false) && ok;
  ok = setRadTLinearEnabled(true) && ok;
  ok = setHighGainMode() && ok;
  ok = enforceAutoFfc(180000) && ok;

  delay(250);
  if (!runFfc()) {
    Serial.println("Lepton CCI warning: FFC run command was not acknowledged");
    ok = false;
  }

  if (readStatus(&status)) {
    Serial.printf("Lepton CCI status after config: 0x%04X\n", status);
  }
  return ok;
}

bool LeptonCci::runFfc() {
  if (!present_) {
    return false;
  }
  Serial.println("Requesting Lepton flat-field correction");
  return runCommand(kSysRunFfc, kCommandRun);
}

bool LeptonCci::setAgcEnabled(bool enabled) {
  if (!present_) {
    return false;
  }
  const bool ok = setCommandEnum32(kAgcEnableState, enabled ? 1 : 0, "AGC enable");
  if (ok) {
    Serial.printf("Lepton AGC %s\n", enabled ? "enabled" : "disabled");
  }
  return ok;
}

bool LeptonCci::setAgcPolicyHeq() {
  if (!present_) {
    return false;
  }
  static constexpr uint32_t kAgcHeq = 1;
  const bool ok = setCommandEnum32(kAgcPolicy, kAgcHeq, "AGC HEQ policy");
  if (ok) {
    Serial.println("Lepton AGC policy set to HEQ");
  }
  return ok;
}

bool LeptonCci::setAgcHeqScale14Bit() {
  if (!present_) {
    return false;
  }
  static constexpr uint32_t kScaleTo14Bits = 1;
  const bool ok = setCommandEnum32(kAgcHeqOutputScaleFactor, kScaleTo14Bits, "AGC HEQ 14-bit scale");
  if (ok) {
    Serial.println("Lepton HEQ output scale set to 14-bit");
  }
  return ok;
}

bool LeptonCci::setAgcCalculationEnabled(bool enabled) {
  if (!present_) {
    return false;
  }
  const bool ok = setCommandEnum32(kAgcCalculationEnableState, enabled ? 1 : 0, "AGC calculation enable");
  if (ok) {
    Serial.printf("Lepton AGC calculation %s\n", enabled ? "enabled" : "disabled");
  }
  return ok;
}

bool LeptonCci::setRadTLinearEnabled(bool enabled) {
  if (!present_) {
    return false;
  }
  const bool ok = setCommandEnum32(kRadTLinearEnableState, enabled ? 1 : 0, "RAD TLinear enable");
  if (ok) {
    Serial.printf("Lepton TLinear radiometric output %s\n", enabled ? "enabled" : "disabled");
  }
  return ok;
}

bool LeptonCci::setHighGainMode() {
  if (!present_) {
    return false;
  }
  static constexpr uint32_t kSysGainModeHigh = 0;
  const bool ok = setCommandEnum32(kSysGainMode, kSysGainModeHigh, "SYS high gain mode");
  if (ok) {
    Serial.println("Lepton gain mode set to High Gain");
  }
  return ok;
}

bool LeptonCci::enforceAutoFfc(uint32_t periodMs) {
  if (!present_) {
    return false;
  }

  static constexpr uint32_t kShutterModeAuto = 1;
  static constexpr uint32_t kTempLockoutInactive = 0;
  static constexpr uint32_t kEnabled = 1;
  static constexpr uint32_t kDisabled = 0;
  static constexpr uint16_t kTempDelta150CentiKelvin = 150;
  static constexpr uint16_t kImminentDelayFrames = 52;

  const uint16_t words[] = {
      static_cast<uint16_t>(kShutterModeAuto & 0xFFFF),
      static_cast<uint16_t>(kShutterModeAuto >> 16),
      static_cast<uint16_t>(kTempLockoutInactive & 0xFFFF),
      static_cast<uint16_t>(kTempLockoutInactive >> 16),
      static_cast<uint16_t>(kEnabled & 0xFFFF),
      static_cast<uint16_t>(kEnabled >> 16),
      static_cast<uint16_t>(kDisabled & 0xFFFF),
      static_cast<uint16_t>(kDisabled >> 16),
      0,
      0,
      static_cast<uint16_t>(periodMs & 0xFFFF),
      static_cast<uint16_t>(periodMs >> 16),
      static_cast<uint16_t>(kDisabled & 0xFFFF),
      static_cast<uint16_t>(kDisabled >> 16),
      kTempDelta150CentiKelvin,
      kImminentDelayFrames,
      0,
  };

  const bool ok = setCommandWords(kSysFfcShutterModeObj,
                                  words,
                                  sizeof(words) / sizeof(words[0]),
                                  "Auto FFC mode");
  if (ok) {
    Serial.printf("Lepton Auto-FFC enforced, period=%lu ms\n", static_cast<unsigned long>(periodMs));
  }
  return ok;
}

bool LeptonCci::readStatus(uint16_t* status) const {
  if (!present_ || status == nullptr) {
    return false;
  }
  return readRegister(kRegStatus, status);
}

bool LeptonCci::writeRegister(uint16_t reg, uint16_t value) const {
  Wire.beginTransmission(kLeptonAddress);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  Wire.write(static_cast<uint8_t>(value >> 8));
  Wire.write(static_cast<uint8_t>(value & 0xFF));
  const uint8_t error = Wire.endTransmission();
  if (error != 0) {
    Serial.printf("Lepton CCI write error: reg=0x%04X value=0x%04X Wire error=%u\n", reg, value, error);
    return false;
  }
  return true;
}

bool LeptonCci::readRegister(uint16_t reg, uint16_t* value) const {
  if (value == nullptr) {
    return false;
  }

  Wire.beginTransmission(kLeptonAddress);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  uint8_t error = Wire.endTransmission(false);
  if (error != 0) {
    Serial.printf("Lepton CCI address error: reg=0x%04X Wire error=%u\n", reg, error);
    return false;
  }

  const uint8_t received = Wire.requestFrom(kLeptonAddress, static_cast<uint8_t>(2));
  if (received != 2) {
    Serial.printf("Lepton CCI read error: reg=0x%04X received=%u\n", reg, received);
    return false;
  }

  *value = (static_cast<uint16_t>(Wire.read()) << 8) | Wire.read();
  return true;
}

bool LeptonCci::writeDataWord(uint16_t index, uint16_t value) const {
  return writeRegister(kRegData0 + (index * 2U), value);
}

bool LeptonCci::writeDataWords(const uint16_t* words, uint16_t wordCount) const {
  if (words == nullptr || wordCount == 0) {
    return false;
  }
  for (uint16_t i = 0; i < wordCount; ++i) {
    if (!writeDataWord(i, words[i])) {
      return false;
    }
  }
  return true;
}

bool LeptonCci::waitUntilReady(uint32_t timeoutMs) const {
  const uint32_t started = millis();
  while (millis() - started < timeoutMs) {
    uint16_t status = 0;
    if (!readRegister(kRegStatus, &status)) {
      return false;
    }
    if ((status & 0x0001U) == 0) {
      return true;
    }
    delay(5);
  }
  Serial.println("Lepton CCI timeout: command interface stayed busy");
  return false;
}

bool LeptonCci::commandSucceeded(const char* name) const {
  uint16_t status = 0;
  if (!readRegister(kRegStatus, &status)) {
    Serial.printf("Lepton CCI %s error: unable to read command status\n", name);
    return false;
  }

  const uint8_t responseError = static_cast<uint8_t>(status >> 8);
  if (responseError != 0) {
    Serial.printf("Lepton CCI %s failed: status=0x%04X responseError=0x%02X\n", name, status, responseError);
    return false;
  }
  return true;
}

bool LeptonCci::runCommand(uint16_t commandBase, uint16_t commandType) const {
  if (!waitUntilReady()) {
    return false;
  }
  if (!writeRegister(kRegDataLength, 0)) {
    return false;
  }
  if (!writeRegister(kRegCommand, commandBase | commandType)) {
    return false;
  }
  return waitUntilReady(750) && commandSucceeded("run command");
}

bool LeptonCci::setCommandWords(uint16_t commandBase,
                                const uint16_t* words,
                                uint16_t wordCount,
                                const char* name) const {
  if (!waitUntilReady()) {
    return false;
  }
  if (!writeDataWords(words, wordCount)) {
    return false;
  }
  if (!writeRegister(kRegDataLength, wordCount)) {
    return false;
  }
  if (!writeRegister(kRegCommand, commandBase | kCommandSet)) {
    return false;
  }
  return waitUntilReady() && commandSucceeded(name);
}

bool LeptonCci::setCommandEnum32(uint16_t commandBase, uint32_t value, const char* name) const {
  const uint16_t words[] = {
      static_cast<uint16_t>(value & 0xFFFF),
      static_cast<uint16_t>(value >> 16),
  };
  return setCommandWords(commandBase, words, sizeof(words) / sizeof(words[0]), name);
}

}  // namespace flir
