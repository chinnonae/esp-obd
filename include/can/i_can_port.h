#pragma once

#include "can/can_config.h"
#include "can/can_frame.h"
#include "can/can_result.h"
#include "can/i_clock.h"

namespace esp_obd::can {

class ICanPort {
 public:
  virtual bool configure(const CanConfig& config) = 0;
  virtual CanResult send(const CanFrame& frame, Milliseconds timeout) = 0;
  // Always non-blocking: returns the next queued frame or reports none.
  // Never sleeps and never carries its own timeout -- any receive deadline
  // is computed by the caller from IClock and driven via poll(now). See
  // docs/ARCHITECTURE.md.
  virtual ReceiveResult receive() = 0;
  virtual CanStatus status() const = 0;
  virtual ~ICanPort() = default;
};

}  // namespace esp_obd::can
