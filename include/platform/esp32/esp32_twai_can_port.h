#pragma once

#include "can/i_can_port.h"

// The only code that converts between CanFrame and twai_message_t; owns
// TWAI driver install/start/stop. See docs/ARCHITECTURE.md's platform
// layer. Not compiled in native_test -- requires ESP-IDF headers.

namespace esp_obd::platform::esp32 {

class Esp32TwaiCanPort : public can::ICanPort {
 public:
  bool configure(const can::CanConfig& config) override;
  can::CanResult send(const can::CanFrame& frame, can::Milliseconds timeout) override;
  can::ReceiveResult receive() override;
  void discardPending() override;
  can::CanStatus status() const override;

 private:
  // Gets a bus-off controller running again (recovery + restart). Returns
  // false if it is still not running within `timeout`.
  bool ensureRunning(can::Milliseconds timeout);
  // Aborts whatever the controller is stuck transmitting by restarting it.
  void restartController(can::Milliseconds timeout);

  bool installed_ = false;
  can::CanConfig currentConfig_{};
};

}  // namespace esp_obd::platform::esp32
