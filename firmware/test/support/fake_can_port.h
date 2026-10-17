#pragma once

#include <deque>
#include <vector>

#include "can/can_config.h"
#include "can/can_frame.h"
#include "can/can_result.h"
#include "can/i_can_port.h"

// Test double implementing the real `ICanPort` (T02). Captures every sent
// frame in order and serves queued frames to a receiver. `receive()` never
// blocks and never sleeps, matching the `ICanPort` contract.
//
// discardPending() timing model: a frame passed to queueRx() counts as
// arriving *after* the next request is transmitted (i.e. it is that
// request's reply), so the next discardPending() keeps it. Anything still
// unread when a later discardPending() runs was already sitting in the
// driver queue before that request, and is dropped -- exactly like a late
// reply to an earlier request on real hardware.
class FakeCanPort : public esp_obd::can::ICanPort {
 public:
  bool configure(const esp_obd::can::CanConfig& config) override {
    lastConfig_ = config;
    configured_ = true;
    return true;
  }

  esp_obd::can::CanResult send(const esp_obd::can::CanFrame& frame,
                                esp_obd::can::Milliseconds /*timeout*/) override {
    txCaptured_.push_back(frame);
    return nextSendResult_;
  }

  esp_obd::can::ReceiveResult receive() override {
    if (rxQueue_.empty()) {
      return esp_obd::can::ReceiveResult{/*hasFrame=*/false, {}};
    }
    esp_obd::can::CanFrame frame = rxQueue_.front().frame;
    rxQueue_.pop_front();
    return esp_obd::can::ReceiveResult{/*hasFrame=*/true, frame};
  }

  void discardPending() override {
    while (!rxQueue_.empty() && rxQueue_.front().epoch < epoch_) {
      rxQueue_.pop_front();
    }
    epoch_++;
    discardCount_++;
  }

  esp_obd::can::CanStatus status() const override { return status_; }

  // Test-only controls, not part of ICanPort.
  void queueRx(const esp_obd::can::CanFrame& frame) { rxQueue_.push_back({frame, epoch_}); }
  void setNextSendResult(esp_obd::can::CanResult result) { nextSendResult_ = result; }
  void setStatus(const esp_obd::can::CanStatus& status) { status_ = status; }

  const std::vector<esp_obd::can::CanFrame>& transmitted() const { return txCaptured_; }
  bool configured() const { return configured_; }
  const esp_obd::can::CanConfig& lastConfig() const { return lastConfig_; }
  size_t discardCount() const { return discardCount_; }

  void reset() {
    txCaptured_.clear();
    rxQueue_.clear();
  }

 private:
  struct QueuedFrame {
    esp_obd::can::CanFrame frame;
    size_t epoch;  // discardPending() calls made before this frame was queued
  };

  std::vector<esp_obd::can::CanFrame> txCaptured_;
  std::deque<QueuedFrame> rxQueue_;
  size_t epoch_ = 0;
  size_t discardCount_ = 0;
  esp_obd::can::CanResult nextSendResult_ = esp_obd::can::CanResult::Ok;
  esp_obd::can::CanStatus status_{};
  esp_obd::can::CanConfig lastConfig_{};
  bool configured_ = false;
};
