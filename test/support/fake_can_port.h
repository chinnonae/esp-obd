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
    esp_obd::can::CanFrame frame = rxQueue_.front();
    rxQueue_.pop_front();
    return esp_obd::can::ReceiveResult{/*hasFrame=*/true, frame};
  }

  esp_obd::can::CanStatus status() const override { return status_; }

  // Test-only controls, not part of ICanPort.
  void queueRx(const esp_obd::can::CanFrame& frame) { rxQueue_.push_back(frame); }
  void setNextSendResult(esp_obd::can::CanResult result) { nextSendResult_ = result; }
  void setStatus(const esp_obd::can::CanStatus& status) { status_ = status; }

  const std::vector<esp_obd::can::CanFrame>& transmitted() const { return txCaptured_; }
  bool configured() const { return configured_; }
  const esp_obd::can::CanConfig& lastConfig() const { return lastConfig_; }

  void reset() {
    txCaptured_.clear();
    rxQueue_.clear();
  }

 private:
  std::vector<esp_obd::can::CanFrame> txCaptured_;
  std::deque<esp_obd::can::CanFrame> rxQueue_;
  esp_obd::can::CanResult nextSendResult_ = esp_obd::can::CanResult::Ok;
  esp_obd::can::CanStatus status_{};
  esp_obd::can::CanConfig lastConfig_{};
  bool configured_ = false;
};
