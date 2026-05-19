#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace flir {

class LeptonCci {
 public:
  bool begin();
  bool isPresent() const;
  bool configureImageQuality();
  bool runFfc();
  bool setAgcEnabled(bool enabled);
  bool setAgcPolicyHeq();
  bool setAgcHeqScale14Bit();
  bool setAgcCalculationEnabled(bool enabled);
  bool setRadTLinearEnabled(bool enabled);
  bool setHighGainMode();
  bool enforceAutoFfc(uint32_t periodMs = 180000);
  bool readStatus(uint16_t* status) const;

 private:
  static constexpr uint8_t kLeptonAddress = 0x2A;
  static constexpr uint32_t kBusFrequency = 400000;
  static constexpr uint16_t kRegStatus = 0x0002;
  static constexpr uint16_t kRegCommand = 0x0004;
  static constexpr uint16_t kRegDataLength = 0x0006;
  static constexpr uint16_t kRegData0 = 0x0008;

  static constexpr uint16_t kCommandGet = 0x0000;
  static constexpr uint16_t kCommandSet = 0x0001;
  static constexpr uint16_t kCommandRun = 0x0002;
  static constexpr uint16_t kAgcEnableState = 0x0100;
  static constexpr uint16_t kAgcPolicy = 0x0104;
  static constexpr uint16_t kAgcHeqOutputScaleFactor = 0x0144;
  static constexpr uint16_t kAgcCalculationEnableState = 0x0148;
  static constexpr uint16_t kSysFfcShutterModeObj = 0x023C;
  static constexpr uint16_t kSysRunFfc = 0x0240;
  static constexpr uint16_t kSysGainMode = 0x0248;
  static constexpr uint16_t kRadTLinearEnableState = 0x4EC0;

  bool writeRegister(uint16_t reg, uint16_t value) const;
  bool readRegister(uint16_t reg, uint16_t* value) const;
  bool writeDataWords(const uint16_t* words, uint16_t wordCount) const;
  bool writeDataWord(uint16_t index, uint16_t value) const;
  bool waitUntilReady(uint32_t timeoutMs = 250) const;
  bool commandSucceeded(const char* name) const;
  bool runCommand(uint16_t commandBase, uint16_t commandType) const;
  bool setCommandWords(uint16_t commandBase, const uint16_t* words, uint16_t wordCount, const char* name) const;
  bool setCommandEnum32(uint16_t commandBase, uint32_t value, const char* name) const;

  bool present_ = false;
};

}  // namespace flir
