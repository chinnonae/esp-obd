#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "core/i_settings_store.h"

// Deliberately separate from ElmSession: ATZ/ATD/ATWS reset the session
// but must NOT wipe persisted settings (that's the whole point of
// persistence -- only ATFE forgets them). ElmCommandEngine holds one of
// these alongside its ElmSession.

namespace esp_obd::elm {

inline constexpr size_t kDeviceIdHexDigits = 12;

struct PersistedSettingsCache {
  // ATM0/ATM1. Not itself persisted to the store -- always starts M1 on
  // load, per docs/tasks/11-settings-persistence-commands.md.
  bool persistenceEnabled = true;

  std::array<char, kDeviceIdHexDigits + 1> deviceId{};  // AT@2/AT@3
  uint8_t savedDataByte = 0;                             // ATRD/ATSDhh

  // Syncs deviceId/savedDataByte from the store. Does not touch
  // persistenceEnabled.
  void loadFrom(const core::ISettingsStore& store) {
    std::strncpy(deviceId.data(), store.deviceId(), kDeviceIdHexDigits);
    deviceId[kDeviceIdHexDigits] = '\0';
    savedDataByte = store.savedDataByte();
  }
};

}  // namespace esp_obd::elm
