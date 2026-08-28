#include "platform/esp32/esp32_settings_store.h"

#include <Preferences.h>

#include <cstring>

#include "core/build_info.h"

namespace esp_obd::platform::esp32 {

namespace {
constexpr const char* kNamespace = "esp_obd";
constexpr const char* kDeviceIdKey = "device_id";
constexpr const char* kSavedByteKey = "saved_byte";
}  // namespace

Esp32SettingsStore::Esp32SettingsStore() {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/true);
  String stored = prefs.getString(kDeviceIdKey, build::kDefaultDeviceId);
  std::strncpy(deviceId_, stored.c_str(), sizeof(deviceId_) - 1);
  deviceId_[sizeof(deviceId_) - 1] = '\0';
  savedDataByte_ = prefs.getUChar(kSavedByteKey, 0);
  prefs.end();
}

const char* Esp32SettingsStore::deviceId() const { return deviceId_; }

void Esp32SettingsStore::setDeviceId(const char* twelveHexDigitsAndNul) {
  std::strncpy(deviceId_, twelveHexDigitsAndNul, sizeof(deviceId_) - 1);
  deviceId_[sizeof(deviceId_) - 1] = '\0';

  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  prefs.putString(kDeviceIdKey, deviceId_);
  prefs.end();
}

uint8_t Esp32SettingsStore::savedDataByte() const { return savedDataByte_; }

void Esp32SettingsStore::setSavedDataByte(uint8_t value) {
  savedDataByte_ = value;

  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  prefs.putUChar(kSavedByteKey, value);
  prefs.end();
}

void Esp32SettingsStore::eraseAll() {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  prefs.clear();
  prefs.end();

  std::strncpy(deviceId_, build::kDefaultDeviceId, sizeof(deviceId_) - 1);
  deviceId_[sizeof(deviceId_) - 1] = '\0';
  savedDataByte_ = 0;
}

}  // namespace esp_obd::platform::esp32
