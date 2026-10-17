#include "isotp/isotp_transmit.h"

#include <algorithm>
#include <array>

namespace esp_obd::isotp {

void IsoTpTransmitter::sendSingleFrame(can::Milliseconds /*now*/, size_t payloadLen) {
  std::array<uint8_t, 8> data{};
  size_t offset = 0;
  if (config_.extendedAddressingEnabled) {
    data[0] = config_.transmitExtendedAddressByte;
    offset = 1;
  }
  data[offset] = static_cast<uint8_t>(payloadLen);
  for (size_t i = 0; i < payloadLen; ++i) {
    data[offset + 1 + i] = payload_[i];
  }

  auto frame = config_.idIsExtendedCan ? can::makeExtendedFrame(config_.id, data, 8)
                                        : can::makeStandardFrame(config_.id, data, 8);
  if (!frame.has_value()) {
    state_ = TxState::ProtocolError;
    return;
  }
  if (port_.send(*frame, config_.sendTimeoutMs) != can::CanResult::Ok) {
    state_ = TxState::BusError;
    return;
  }
  bytesSent_ = payloadLen;
  state_ = TxState::Complete;
}

void IsoTpTransmitter::sendFirstFrame(can::Milliseconds now) {
  std::array<uint8_t, 8> data{};
  size_t offset = 0;
  if (config_.extendedAddressingEnabled) {
    data[0] = config_.transmitExtendedAddressByte;
    offset = 1;
  }
  uint16_t len = static_cast<uint16_t>(payloadLen_);
  data[offset] = static_cast<uint8_t>(0x10 | ((len >> 8) & 0x0F));
  data[offset + 1] = static_cast<uint8_t>(len & 0xFF);

  size_t initialCapacity = 8 - offset - 2;
  size_t initialBytes = std::min(initialCapacity, payloadLen_);
  for (size_t i = 0; i < initialBytes; ++i) {
    data[offset + 2 + i] = payload_[i];
  }

  auto frame = config_.idIsExtendedCan ? can::makeExtendedFrame(config_.id, data, 8)
                                        : can::makeStandardFrame(config_.id, data, 8);
  if (!frame.has_value()) {
    state_ = TxState::ProtocolError;
    return;
  }
  if (port_.send(*frame, config_.sendTimeoutMs) != can::CanResult::Ok) {
    state_ = TxState::BusError;
    return;
  }
  bytesSent_ = initialBytes;
  nextSequence_ = 1;
  state_ = TxState::WaitingForFlowControl;
  deadline_ = now + config_.flowControlTimeoutMs;
}

void IsoTpTransmitter::sendNextConsecutiveFrame(can::Milliseconds now) {
  std::array<uint8_t, 8> data{};
  size_t offset = 0;
  if (config_.extendedAddressingEnabled) {
    data[0] = config_.transmitExtendedAddressByte;
    offset = 1;
  }
  data[offset] = static_cast<uint8_t>(0x20 | (nextSequence_ & 0x0F));

  size_t capacity = 8 - offset - 1;
  size_t remaining = payloadLen_ - bytesSent_;
  size_t take = std::min(capacity, remaining);
  for (size_t i = 0; i < take; ++i) {
    data[offset + 1 + i] = payload_[bytesSent_ + i];
  }

  auto frame = config_.idIsExtendedCan ? can::makeExtendedFrame(config_.id, data, 8)
                                        : can::makeStandardFrame(config_.id, data, 8);
  if (!frame.has_value()) {
    state_ = TxState::ProtocolError;
    return;
  }
  if (port_.send(*frame, config_.sendTimeoutMs) != can::CanResult::Ok) {
    state_ = TxState::BusError;
    return;
  }

  bytesSent_ += take;
  nextSequence_ = (nextSequence_ == 15) ? 0 : static_cast<uint8_t>(nextSequence_ + 1);
  cfsSentInBlock_++;

  if (bytesSent_ >= payloadLen_) {
    state_ = TxState::Complete;
    return;
  }

  if (blockSize_ > 0 && cfsSentInBlock_ >= blockSize_) {
    state_ = TxState::WaitingForFlowControl;
    deadline_ = now + config_.flowControlTimeoutMs;
    return;
  }

  nextSendTime_ = now + stMinMs_;
}

void IsoTpTransmitter::start(can::Milliseconds now, const uint8_t* payload, size_t payloadLen) {
  payload_ = payload;
  payloadLen_ = payloadLen;
  bytesSent_ = 0;
  nextSequence_ = 1;
  blockSize_ = 0;
  cfsSentInBlock_ = 0;
  stMinMs_ = 0;

  if (payloadLen == 0 || payloadLen > kMaxPayloadBytes) {
    state_ = TxState::ProtocolError;
    return;
  }

  size_t singleFrameCapacity = frameCapacity() - 1;  // 1 byte spent on SF's PCI
  if (payloadLen <= singleFrameCapacity) {
    sendSingleFrame(now, payloadLen);
  } else {
    sendFirstFrame(now);
  }
}

void IsoTpTransmitter::onFlowControl(can::Milliseconds now, const can::CanFrame& frame) {
  if (state_ != TxState::WaitingForFlowControl) {
    return;
  }

  const uint8_t* window = frame.data.data();
  size_t windowLen = frame.dlc;
  if (config_.extendedAddressingEnabled) {
    if (frame.dlc < 1 || frame.data[0] != config_.requiredExtendedAddressByte) {
      return;  // ignored, not an error -- see the RX side's ATCERhh handling
    }
    window = frame.data.data() + 1;
    windowLen = frame.dlc - 1;
  }

  auto parsed = parsePci(window, windowLen);
  if (!parsed.has_value() || parsed->type != PciType::FlowControl) {
    state_ = TxState::ProtocolError;
    return;
  }

  switch (parsed->flowStatus) {
    case FlowStatus::Overflow:
      state_ = TxState::Overflow;
      return;
    case FlowStatus::Wait:
      deadline_ = now + config_.flowControlTimeoutMs;
      return;
    case FlowStatus::ContinueToSend:
      blockSize_ = parsed->blockSize;
      cfsSentInBlock_ = 0;
      stMinMs_ = stMinToMilliseconds(parsed->stMin);
      state_ = TxState::SendingConsecutiveFrames;
      nextSendTime_ = now;  // the first CF after an FC may go immediately
      return;
  }
}

void IsoTpTransmitter::poll(can::Milliseconds now) {
  if (state_ == TxState::WaitingForFlowControl) {
    if (now >= deadline_) {
      state_ = TxState::TimedOut;
    }
    return;
  }
  if (state_ != TxState::SendingConsecutiveFrames) {
    return;
  }
  if (now < nextSendTime_) {
    return;
  }
  sendNextConsecutiveFrame(now);
}

}  // namespace esp_obd::isotp
