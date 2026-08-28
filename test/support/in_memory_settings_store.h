#pragma once

#include <cstring>

#include "core/build_info.h"
#include "core/i_settings_store.h"

// Test double for esp_obd::core::ISettingsStore: keeps everything in RAM.
// Not a description of the real storage -- T07's Esp32SettingsStore backs
// the same interface with ESP32 NVS.
class InMemorySettingsStore : public esp_obd::core::ISettingsStore {
 public:
  InMemorySettingsStore() { eraseAll(); }

  const char* deviceId() const override { return deviceId_; }

  void setDeviceId(const char* twelveHexDigitsAndNul) override {
    std::strncpy(deviceId_, twelveHexDigitsAndNul, sizeof(deviceId_) - 1);
    deviceId_[sizeof(deviceId_) - 1] = '\0';
  }

  uint8_t savedDataByte() const override { return savedDataByte_; }
  void setSavedDataByte(uint8_t value) override { savedDataByte_ = value; }

  void eraseAll() override {
    std::strncpy(deviceId_, esp_obd::build::kDefaultDeviceId, sizeof(deviceId_) - 1);
    deviceId_[sizeof(deviceId_) - 1] = '\0';
    savedDataByte_ = 0;
  }

 private:
  char deviceId_[13] = {};
  uint8_t savedDataByte_ = 0;
};
