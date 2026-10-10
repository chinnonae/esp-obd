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
  // Drops every frame already waiting to be returned by receive(). Called
  // right before a diagnostic request is transmitted, so a late reply to an
  // earlier request can't be mistaken for a reply to this one.
  virtual void discardPending() = 0;
  virtual CanStatus status() const = 0;
  virtual ~ICanPort() = default;
};

}  // namespace esp_obd::can
