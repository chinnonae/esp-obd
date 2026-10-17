#pragma once

#include <cstdint>

// Deterministic test clock: time advances only when explicitly told to.
// Will implement the `IClock` interface once T02 defines it in `can/`.
class FakeClock {
 public:
  uint32_t now() const { return nowMs_; }

  void advance(uint32_t deltaMs) { nowMs_ += deltaMs; }

  void setNow(uint32_t ms) { nowMs_ = ms; }

 private:
  uint32_t nowMs_ = 0;
};
