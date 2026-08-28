#include "isotp/isotp_receive.h"

#include <algorithm>

namespace esp_obd::isotp {

bool IsoTpReceiver::resolveWindow(const can::CanFrame& frame, const uint8_t** windowData,
                                   size_t* windowLen) const {
  if (config_.extendedAddressingEnabled) {
    if (frame.dlc < 1 || frame.data[0] != config_.requiredExtendedAddressByte) {
      return false;
    }
    *windowData = frame.data.data() + 1;
    *windowLen = frame.dlc - 1;
  } else {
    *windowData = frame.data.data();
    *windowLen = frame.dlc;
  }
  return true;
}

void IsoTpReceiver::recordRawFrame(const can::CanFrame& frame) {
  if (rawFrameCount_ < rawFrames_.size()) {
    rawFrames_[rawFrameCount_++] = frame;
  }
}

void IsoTpReceiver::sendFlowControl(FlowStatus status) {
  std::array<uint8_t, 8> data{};
  size_t offset = 0;
  if (config_.extendedAddressingEnabled) {
    data[0] = config_.transmitExtendedAddressByte;
    offset = 1;
  }
  buildFlowControlPci(data.data() + offset, status, config_.flowControlBlockSize,
                       config_.flowControlStMin);

  // Padded to a full 8-byte classical CAN frame, matching the padding
  // convention shown throughout docs/ELM_COMMAND_BEHAVIOR.md's examples.
  // T09's ATV0/ATV1 may need to make this configurable.
  auto frame = config_.flowControlIdIsExtendedCan
                   ? can::makeExtendedFrame(config_.flowControlId, data, 8)
                   : can::makeStandardFrame(config_.flowControlId, data, 8);
  if (!frame.has_value()) {
    state_ = RxState::BusError;
    return;
  }
  if (port_.send(*frame, config_.frameTimeoutMs) != can::CanResult::Ok) {
    state_ = RxState::BusError;
  }
}

void IsoTpReceiver::start(can::Milliseconds now, const can::CanFrame& frame) {
  const uint8_t* window = nullptr;
  size_t windowLen = 0;
  if (!resolveWindow(frame, &window, &windowLen)) {
    return;  // extended-address mismatch: ignored, not an error
  }

  // Reset for a new transaction (this instance may be reused).
  state_ = RxState::Idle;
  payloadDeclaredLength_ = 0;
  payloadReceived_ = 0;
  expectedSequence_ = 1;
  cfsSinceFlowControl_ = 0;
  rawFrameCount_ = 0;

  auto parsed = parsePci(window, windowLen);
  if (!parsed.has_value()) {
    state_ = RxState::ProtocolError;
    return;
  }
  recordRawFrame(frame);

  if (parsed->type == PciType::SingleFrame) {
    size_t length = parsed->length;
    if (length == 0 || length > 7 || windowLen < 1 + length) {
      state_ = RxState::ProtocolError;
      return;
    }
    std::copy(window + 1, window + 1 + length, payload_.begin());
    payloadDeclaredLength_ = length;
    payloadReceived_ = length;
    state_ = RxState::Complete;
    return;
  }

  if (parsed->type == PciType::FirstFrame) {
    size_t length = parsed->length;
    if (length < 8 || length > kMaxPayloadBytes || windowLen < 2) {
      state_ = RxState::ProtocolError;
      return;
    }
    size_t initialBytes = std::min(windowLen - 2, length);
    std::copy(window + 2, window + 2 + initialBytes, payload_.begin());
    payloadDeclaredLength_ = length;
    payloadReceived_ = initialBytes;
    expectedSequence_ = 1;
    state_ = RxState::ReceivingConsecutiveFrames;
    deadline_ = now + config_.frameTimeoutMs;

    if (config_.sendAutomaticFlowControl) {
      sendFlowControl(FlowStatus::ContinueToSend);
    }
    return;
  }

  // A Consecutive Frame or Flow Control frame cannot start a reception.
  state_ = RxState::ProtocolError;
}

void IsoTpReceiver::onFrame(can::Milliseconds now, const can::CanFrame& frame) {
  if (state_ != RxState::ReceivingConsecutiveFrames) {
    return;
  }

  const uint8_t* window = nullptr;
  size_t windowLen = 0;
  if (!resolveWindow(frame, &window, &windowLen)) {
    return;  // extended-address mismatch: ignored, not an error
  }

  auto parsed = parsePci(window, windowLen);
  if (!parsed.has_value() || parsed->type != PciType::ConsecutiveFrame ||
      parsed->sequence != expectedSequence_) {
    state_ = RxState::ProtocolError;
    return;
  }
  recordRawFrame(frame);

  size_t remaining = payloadDeclaredLength_ - payloadReceived_;
  size_t available = windowLen > 1 ? windowLen - 1 : 0;
  size_t take = std::min(remaining, available);
  std::copy(window + 1, window + 1 + take, payload_.begin() + payloadReceived_);
  payloadReceived_ += take;

  expectedSequence_ = (expectedSequence_ == 15) ? 0 : static_cast<uint8_t>(expectedSequence_ + 1);

  if (payloadReceived_ >= payloadDeclaredLength_) {
    state_ = RxState::Complete;
    return;
  }

  deadline_ = now + config_.frameTimeoutMs;

  cfsSinceFlowControl_++;
  if (config_.flowControlBlockSize > 0 && cfsSinceFlowControl_ >= config_.flowControlBlockSize) {
    cfsSinceFlowControl_ = 0;
    if (config_.sendAutomaticFlowControl) {
      sendFlowControl(FlowStatus::ContinueToSend);
    }
  }
}

void IsoTpReceiver::poll(can::Milliseconds now) {
  if (state_ != RxState::ReceivingConsecutiveFrames) {
    return;
  }
  if (now >= deadline_) {
    state_ = RxState::TimedOut;
  }
}

}  // namespace esp_obd::isotp
