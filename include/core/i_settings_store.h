#pragma once

#include <cstdint>

// Abstract persistence for the small set of settings the ELM contract
// persists (see docs/tasks/11-settings-persistence-commands.md). A
// concrete implementation backs this with real storage: T07's
// Esp32SettingsStore uses ESP32 NVS. The in-memory fake used by native
// tests lives under test/support/, not here -- this file only defines the
// interface shape.

namespace esp_obd::core {

class ISettingsStore {
 public:
  // 12 hex digits + NUL. Returns the stored value, or the compiled-in
  // default (esp_obd::build::kDefaultDeviceId) if never set.
  virtual const char* deviceId() const = 0;

  // Stores `twelveHexDigitsAndNul` verbatim. The caller (the AT@3 handler)
  // validates exactly 12 hex digits before calling; this does not
  // re-validate.
  virtual void setDeviceId(const char* twelveHexDigitsAndNul) = 0;

  virtual uint8_t savedDataByte() const = 0;
  virtual void setSavedDataByte(uint8_t value) = 0;

  // Erases every persisted field above, restoring their defaults.
  virtual void eraseAll() = 0;

  virtual ~ISettingsStore() = default;
};

}  // namespace esp_obd::core
