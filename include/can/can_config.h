#pragma once

// Portable CAN controller configuration. No ESP-IDF/TWAI type may appear
// here -- see docs/ARCHITECTURE.md's dependency rules. Physical wiring
// (which GPIOs) is a fixed hardware constant (T00), not a runtime config
// knob, so it is intentionally not a field here.

namespace esp_obd::can {

enum class Bitrate {
  Bitrate250k,
  Bitrate500k,
};

enum class ControllerMode {
  Normal,
  ListenOnly,
};

struct CanConfig {
  Bitrate bitrate = Bitrate::Bitrate500k;
  ControllerMode mode = ControllerMode::Normal;
};

}  // namespace esp_obd::can
