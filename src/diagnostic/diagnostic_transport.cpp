#include "diagnostic/diagnostic_transport.h"

#include <algorithm>

namespace esp_obd::diagnostic {

namespace {
bool isFunctionalRequest(uint32_t requestId, can::obd::ObdCanProtocol protocol) {
  return requestId == can::obd::functionalRequestId(protocol);
}
}  // namespace

void DiagnosticTransport::start(can::Milliseconds now, const DiagnosticRequest& request,
                                 can::obd::ObdCanProtocol protocol) {
  request_ = request;
  autoSearchActive_ = false;
  finished_ = false;
  result_ = DiagnosticResult{};
  effectiveMaxResponses_ = request_.maxResponses.value_or(
      isFunctionalRequest(request_.requestId, protocol) ? kMaxResponders : 1);
  beginAttempt(now, protocol);
}

void DiagnosticTransport::startAutoSearch(can::Milliseconds now, const DiagnosticRequest& request) {
  request_ = request;
  autoSearchActive_ = true;
  autoSearchIndex_ = 0;
  finished_ = false;
  result_ = DiagnosticResult{};

  can::obd::ObdCanProtocol first = can::obd::kAutoSearchOrder[0];
  request_.requestId = can::obd::functionalRequestId(first);
  request_.requestIdIsExtendedCan = can::obd::isExtendedCan(first);
  // Auto-search only needs one confirming response to lock a protocol; a
  // full multi-ECU functional collection happens on a later request made
  // through start() once already connected.
  effectiveMaxResponses_ = request_.maxResponses.value_or(1);

  can::CanConfig canConfig;
  canConfig.bitrate = can::obd::bitrateFor(first);
  canConfig.mode = can::ControllerMode::Normal;
  if (!port_.configure(canConfig)) {
    finishWithOutcome(DiagnosticOutcome::BusError);
    return;
  }
  beginAttempt(now, first);
}

void DiagnosticTransport::beginAttempt(can::Milliseconds now, can::obd::ObdCanProtocol protocol) {
  currentProtocol_ = protocol;
  activeReceiverCount_ = 0;
  for (auto& r : receivers_) {
    r.reset();
  }

  isotp::TxConfig txConfig;
  txConfig.id = request_.requestId;
  txConfig.idIsExtendedCan = request_.requestIdIsExtendedCan;
  txConfig.extendedAddressingEnabled = request_.extendedAddressingEnabled;
  txConfig.transmitExtendedAddressByte = request_.transmitExtendedAddressByte;
  txConfig.requiredExtendedAddressByte = request_.requiredExtendedAddressByte;
  txConfig.sendTimeoutMs = request_.responseTimeoutMs;
  txConfig.flowControlTimeoutMs = request_.responseTimeoutMs;

  transmitter_.emplace(port_, txConfig);
  transmitting_ = true;
  transmitter_->start(now, request_.payload, request_.payloadLength);
  advanceTransmitPhase(now);
}

void DiagnosticTransport::advanceTransmitPhase(can::Milliseconds now) {
  isotp::TxState state = transmitter_->state();
  if (state == isotp::TxState::Complete) {
    transmitting_ = false;
    overallDeadline_ = now + request_.responseTimeoutMs;
  } else if (state == isotp::TxState::BusError || state == isotp::TxState::ProtocolError ||
             state == isotp::TxState::TimedOut || state == isotp::TxState::Overflow) {
    finishWithOutcome(DiagnosticOutcome::BusError);
  }
  // WaitingForFlowControl / SendingConsecutiveFrames: keep polling.
}

bool DiagnosticTransport::acceptFrame(uint32_t id) const {
  if (request_.explicitReceiveAddress.has_value()) {
    return id == *request_.explicitReceiveAddress;
  }
  if (request_.receiveFilter.has_value()) {
    return can::matchesFilter(id, *request_.receiveFilter);
  }
  return can::obd::isDefaultObdResponse(id, currentProtocol_);
}

void DiagnosticTransport::routeFrame(can::Milliseconds now, const can::CanFrame& frame) {
  if (!acceptFrame(frame.id)) {
    return;
  }

  for (size_t i = 0; i < activeReceiverCount_; ++i) {
    if (receiverSourceIds_[i] == frame.id) {
      receivers_[i]->onFrame(now, frame);
      return;
    }
  }
  if (activeReceiverCount_ >= kMaxResponders) {
    return;
  }

  bool extended = can::obd::isExtendedCan(currentProtocol_);
  isotp::RxConfig rxConfig;
  rxConfig.extendedAddressingEnabled = request_.extendedAddressingEnabled;
  rxConfig.requiredExtendedAddressByte = request_.requiredExtendedAddressByte;
  rxConfig.transmitExtendedAddressByte = request_.transmitExtendedAddressByte;
  rxConfig.sendAutomaticFlowControl = request_.sendAutomaticFlowControl;
  rxConfig.frameTimeoutMs = request_.responseTimeoutMs;
  rxConfig.flowControlId = can::obd::computeFlowControlId(frame.id, extended);
  rxConfig.flowControlIdIsExtendedCan = extended;
  rxConfig.flowControlBlockSize = request_.flowControlBlockSize;
  rxConfig.flowControlStMin = request_.flowControlStMin;

  size_t slot = activeReceiverCount_++;
  receiverSourceIds_[slot] = frame.id;
  receivers_[slot].emplace(port_, rxConfig);
  receivers_[slot]->start(now, frame);
}

void DiagnosticTransport::poll(can::Milliseconds now) {
  if (finished_) {
    return;
  }

  can::ReceiveResult rx = port_.receive();

  if (transmitting_) {
    if (rx.hasFrame) {
      transmitter_->onFlowControl(now, rx.frame);
    }
    transmitter_->poll(now);
    advanceTransmitPhase(now);
    return;
  }

  if (rx.hasFrame) {
    routeFrame(now, rx.frame);
  }

  size_t completeCount = 0;
  for (size_t i = 0; i < activeReceiverCount_; ++i) {
    receivers_[i]->poll(now);
    if (receivers_[i]->state() == isotp::RxState::Complete) {
      completeCount++;
    }
  }

  if (completeCount >= effectiveMaxResponses_ || now >= overallDeadline_) {
    finalizeCollection(now, completeCount);
  }
}

void DiagnosticTransport::finalizeCollection(can::Milliseconds now, size_t completeCount) {
  if (completeCount == 0) {
    if (autoSearchActive_) {
      advanceAutoSearch(now);
    } else {
      finishWithOutcome(DiagnosticOutcome::NoData);
    }
    return;
  }

  bool extended = can::obd::isExtendedCan(currentProtocol_);
  result_.responderCount = 0;
  for (size_t i = 0; i < activeReceiverCount_ && result_.responderCount < kMaxResponders; ++i) {
    if (receivers_[i]->state() != isotp::RxState::Complete) {
      continue;
    }
    Responder& r = result_.responders[result_.responderCount++];
    r.sourceId = receiverSourceIds_[i];
    r.extended = extended;
    r.payloadLength = receivers_[i]->payloadLength();
    std::copy(receivers_[i]->payload(), receivers_[i]->payload() + r.payloadLength,
              r.payload.begin());
    r.rawFrameCount = receivers_[i]->rawFrameCount();
    std::copy(receivers_[i]->rawFrames(), receivers_[i]->rawFrames() + r.rawFrameCount,
              r.rawFrames.begin());
  }

  if (autoSearchActive_) {
    result_.connectedProtocol = currentProtocol_;
  }
  finishWithOutcome(DiagnosticOutcome::Complete);
}

void DiagnosticTransport::advanceAutoSearch(can::Milliseconds now) {
  autoSearchIndex_++;
  if (autoSearchIndex_ >= 4) {
    finishWithOutcome(DiagnosticOutcome::UnableToConnect);
    return;
  }

  can::obd::ObdCanProtocol next = can::obd::kAutoSearchOrder[autoSearchIndex_];
  request_.requestId = can::obd::functionalRequestId(next);
  request_.requestIdIsExtendedCan = can::obd::isExtendedCan(next);

  can::CanConfig canConfig;
  canConfig.bitrate = can::obd::bitrateFor(next);
  canConfig.mode = can::ControllerMode::Normal;
  if (!port_.configure(canConfig)) {
    finishWithOutcome(DiagnosticOutcome::BusError);
    return;
  }
  beginAttempt(now, next);
}

void DiagnosticTransport::finishWithOutcome(DiagnosticOutcome outcome) {
  result_.outcome = outcome;
  finished_ = true;
}

}  // namespace esp_obd::diagnostic
