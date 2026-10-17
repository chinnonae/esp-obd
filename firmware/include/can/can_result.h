#pragma once

#include <cstdint>

#include "can/can_config.h"
#include "can/can_frame.h"

namespace esp_obd::can {

enum class CanResult {
  Ok,
  Timeout,
  BusError,
};

// receive() is always non-blocking (see ICanPort in i_can_port.h): either a
// queued frame is returned, or hasFrame is false.
struct ReceiveResult {
  bool hasFrame = false;
  CanFrame frame{};
};

// Typed status for e.g. ATCS, rather than exposing raw TWAI driver output.
struct CanStatus {
  uint32_t txErrorCounter = 0;
  uint32_t rxErrorCounter = 0;
  bool busOff = false;
  Bitrate configuredBitrate = Bitrate::Bitrate500k;
};

}  // namespace esp_obd::can
