#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

// Test-only CAN frame shape, matching the `CanFrame` type planned in
// docs/ARCHITECTURE.md so T02 can adopt this file directly instead of
// redesigning it. Do not add a field here that isn't already in that struct.
struct FakeCanFrame {
  uint32_t id = 0;
  bool extended = false;
  bool remoteRequest = false;
  uint8_t dlc = 0;
  std::array<uint8_t, 8> data{};

  bool operator==(const FakeCanFrame& other) const {
    return id == other.id && extended == other.extended &&
           remoteRequest == other.remoteRequest && dlc == other.dlc &&
           data == other.data;
  }
};

// Test double standing in for the eventual `ICanPort` (T02). Captures every
// sent frame in order and serves queued frames to a receiver. `receive()`
// never blocks and never sleeps: it either returns the next queued frame or
// reports none, matching the non-blocking `ICanPort::receive()` contract in
// docs/ARCHITECTURE.md.
class FakeCanPort {
 public:
  void send(const FakeCanFrame& frame) { txCaptured_.push_back(frame); }

  void queueRx(const FakeCanFrame& frame) { rxQueue_.push_back(frame); }

  bool receive(FakeCanFrame& outFrame) {
    if (rxQueue_.empty()) {
      return false;
    }
    outFrame = rxQueue_.front();
    rxQueue_.pop_front();
    return true;
  }

  const std::vector<FakeCanFrame>& transmitted() const { return txCaptured_; }

  void reset() {
    txCaptured_.clear();
    rxQueue_.clear();
  }

 private:
  std::vector<FakeCanFrame> txCaptured_;
  std::deque<FakeCanFrame> rxQueue_;
};
