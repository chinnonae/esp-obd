#pragma once

// Single source of truth for firmware/identity strings. Code and native
// tests must both read these constants rather than each spelling out their
// own copy. See docs/tasks/00-project-baseline.md.

namespace esp_obd::build {

inline constexpr char kFirmwareVersion[] = "0.1.0";

// Returned by ATZ/ATI. See docs/ELM_COMMAND_BEHAVIOR.md sections 1.3, 2.1.
inline constexpr char kElmVersion[] = "ELM327 v2.2";

// Fixed AT@1 response text.
inline constexpr char kAdapterDescription[] = "ESP-OBD CAN Adapter";

// AT@2 default device identifier before AT@3 stores one: 12 hex digits.
inline constexpr char kDefaultDeviceId[] = "FFFFFFFFFFFF";

static_assert(sizeof(kDefaultDeviceId) - 1 == 12,
              "AT@2 default device id must be exactly 12 hex digits");

}  // namespace esp_obd::build
