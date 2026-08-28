#pragma once

#include "core/i_settings_store.h"

// Implements ISettingsStore (T11) against ESP32 NVS via the Arduino
// Preferences library. Arduino String stays entirely inside the .cpp; it
// never crosses this interface.

namespace esp_obd::platform::esp32 {

class Esp32SettingsStore : public core::ISettingsStore {
 public:
  Esp32SettingsStore();

  const char* deviceId() const override;
  void setDeviceId(const char* twelveHexDigitsAndNul) override;
  uint8_t savedDataByte() const override;
  void setSavedDataByte(uint8_t value) override;
  void eraseAll() override;

 private:
  char deviceId_[13] = {};
  uint8_t savedDataByte_ = 0;
};

}  // namespace esp_obd::platform::esp32
