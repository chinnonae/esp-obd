#pragma once

#include "core/i_settings_store.h"

// Implements ISettingsStore (T11) against ESP32 NVS via the Arduino
// Preferences library. Arduino String stays entirely inside the .cpp; it
// never crosses this interface.
//
// The constructor does NOT touch NVS: this type is constructed as a file-
// scope global (see main.cpp), whose constructor runs during C++ static
// initialization, before nvs_flash_init() has any chance to run -- calling
// Preferences::begin() that early fails with NOT_INITIALIZED (confirmed
// against real hardware). Call load() explicitly from setup(), after
// nvs_flash_init().

namespace esp_obd::platform::esp32 {

class Esp32SettingsStore : public core::ISettingsStore {
 public:
  Esp32SettingsStore();  // sets in-memory defaults only; no NVS access

  // Reads the persisted values from NVS. The caller must have already
  // called nvs_flash_init() (and handled a first-boot/erase if needed).
  void load();

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
