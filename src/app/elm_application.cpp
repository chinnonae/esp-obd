#include "app/elm_application.h"

#include <cstdio>

#include "can/can_config.h"
#include "can/obd_addresses.h"
#include "elm/elm_errors.h"
#include "elm/elm_formatter.h"
#include "elm/elm_parser.h"
#include "elm/protocol_mapping.h"

namespace esp_obd::app {

elm::ElmReply ElmApplication::execute(can::Milliseconds now, const char* rawLine) {
  bool wasMonitoring = engine_.session().monitorActive;
  elm::ElmReply reply = engine_.execute(rawLine);

  if (reply.kind == elm::ElmReplyKind::DiagnosticRequest) {
    startDiagnostic(now, reply);
    return reply;
  }
  if (reply.kind == elm::ElmReplyKind::CanStatusRequest) {
    return resolveCanStatusRequest();
  }
  if (reply.kind == elm::ElmReplyKind::SendRtrRequest) {
    return resolveSendRtrRequest();
  }
  if (!wasMonitoring && engine_.session().monitorActive) {
    syncMonitorCanMode();
  }
  return reply;
}

void ElmApplication::startDiagnostic(can::Milliseconds now, const elm::ElmReply& requestReply) {
  size_t payloadLen = elm::decodeHexBytes(requestReply.text.c_str(), pendingPayload_.data(),
                                           pendingPayload_.size());

  const elm::ElmSession& session = engine_.session();

  diagnostic::DiagnosticRequest request;
  request.payload = pendingPayload_.data();
  request.payloadLength = payloadLen;
  request.maxResponses = requestReply.diagnosticMaxResponses;
  request.explicitReceiveAddress = session.receiveAddress;
  request.receiveFilter = session.idFilter;
  request.extendedAddressingEnabled = session.extendedAddressingEnabled;
  request.transmitExtendedAddressByte = session.extendedAddressByte;
  request.requiredExtendedAddressByte =
      session.requiredExtendedAddressByte.value_or(session.extendedAddressByte);
  // FCSM1/FCSM2 (manual modes) aren't fully wired to isotp/'s manual FC
  // byte injection yet -- they just disable automatic FC, same as CFC0.
  // See this task's Notes.
  request.sendAutomaticFlowControl = session.flowControlMode == elm::FlowControlMode::Automatic
                                          ? session.automaticFlowControlEnabled
                                          : false;
  request.responseTimeoutMs = session.responseTimeoutMs;

  diagnosticPending_ = true;

  auto fixedProtocol = elm::toObdCanProtocol(session.protocol);
  if (!session.protocolConnected || !fixedProtocol.has_value()) {
    diagnosticTransport_.startAutoSearch(now, request);
  } else {
    if (session.customHeaderId.has_value()) {
      request.requestId = *session.customHeaderId;
      request.requestIdIsExtendedCan = *session.customHeaderId > can::kStandardIdMax;
    } else {
      request.requestId = can::obd::functionalRequestId(*fixedProtocol);
      request.requestIdIsExtendedCan = can::obd::isExtendedCan(*fixedProtocol);
    }
    diagnosticTransport_.start(now, request, *fixedProtocol);
  }

  // start()/startAutoSearch() only send the request; poll() is what
  // drains ICanPort::receive(). Poll once immediately so a response
  // already queued/available completes within this same call, matching
  // how a Single Frame TX itself completes synchronously.
  if (!diagnosticTransport_.finished()) {
    diagnosticTransport_.poll(now);
  }

  if (diagnosticTransport_.finished()) {
    readyReply_ = formatDiagnosticResult(diagnosticTransport_.result());
    diagnosticPending_ = false;
  }
}

bool ElmApplication::poll(can::Milliseconds now) {
  if (!diagnosticPending_) {
    return false;
  }
  diagnosticTransport_.poll(now);
  if (!diagnosticTransport_.finished()) {
    return false;
  }
  readyReply_ = formatDiagnosticResult(diagnosticTransport_.result());
  diagnosticPending_ = false;
  return true;
}

elm::ElmReply ElmApplication::formatDiagnosticResult(const diagnostic::DiagnosticResult& result) {
  if (result.connectedProtocol.has_value()) {
    // Persist the auto-search outcome so later requests skip re-searching.
    engine_.session().protocol = elm::fromObdCanProtocol(*result.connectedProtocol);
    engine_.session().protocolConnected = true;
    engine_.session().protocolDiscoveredViaAutoSearch = true;
  }

  if (result.responderCount > 0) {
    const diagnostic::Responder& last = result.responders[result.responderCount - 1];
    if (last.rawFrameCount > 0) {
      engine_.session().lastAcceptedReceivedFrame = last.rawFrames[last.rawFrameCount - 1];
    }
  }

  const elm::ElmSession& baseSession = engine_.session();

  switch (result.outcome) {
    case diagnostic::DiagnosticOutcome::NoData:
      return elm::textReply(baseSession, elm::kNoDataText);
    case diagnostic::DiagnosticOutcome::BusError:
      return elm::textReply(baseSession, elm::kCanErrorText);
    case diagnostic::DiagnosticOutcome::UnableToConnect:
      return elm::textReply(baseSession, elm::kUnableToConnectText);
    case diagnostic::DiagnosticOutcome::Complete:
      break;
  }

  // "More than one complete responder forces headers on" (contract
  // section 1.4), even if ATH0 is selected.
  elm::ElmSession renderSession = baseSession;
  if (result.responderCount > 1) {
    renderSession.headersEnabled = true;
  }

  // Simplification: renders one line per responder using its first raw
  // frame for header/DLC display. A full multi-frame ATH1 (one line per
  // raw CF, PCI-declared-length trimming under ATD0) is left for a later
  // pass.
  elm::ElmReplyText body;
  for (size_t i = 0; i < result.responderCount; ++i) {
    const diagnostic::Responder& responder = result.responders[i];
    if (i > 0) {
      body += elm::responseEnding(renderSession);
    }
    can::CanFrame representativeFrame;
    if (responder.rawFrameCount > 0) {
      representativeFrame = responder.rawFrames[0];
    } else {
      representativeFrame.id = responder.sourceId;
      representativeFrame.extended = responder.extended;
    }
    body += elm::formatResponseBody(renderSession, representativeFrame, responder.payload.data(),
                                     responder.payloadLength, representativeFrame.dlc)
                .c_str();
  }

  elm::ElmReply reply;
  reply.kind = elm::ElmReplyKind::Text;
  reply.text = body;
  reply.text += elm::responseEnding(renderSession);
  reply.appendPrompt = true;
  return reply;
}

elm::ElmReply ElmApplication::resolveCanStatusRequest() {
  can::CanStatus status = canPort_.status();
  const char* bitrateText = status.configuredBitrate == can::Bitrate::Bitrate500k ? "500K" : "250K";
  uint32_t tx = status.txErrorCounter > 0xFF ? 0xFF : status.txErrorCounter;
  uint32_t rx = status.rxErrorCounter > 0xFF ? 0xFF : status.rxErrorCounter;
  char buf[64];
  std::snprintf(buf, sizeof(buf), "TXERR:%02X RXERR:%02X BUSOFF:%d RATE:%s", tx, rx,
                status.busOff ? 1 : 0, bitrateText);
  return elm::textReply(engine_.session(), buf);
}

elm::ElmReply ElmApplication::resolveSendRtrRequest() {
  const elm::ElmSession& session = engine_.session();
  uint32_t id = session.customHeaderId.value_or(session.requestId);
  bool extended = id > can::kStandardIdMax;
  auto frame = extended ? can::makeExtendedRemoteFrame(id, 0) : can::makeStandardRemoteFrame(id, 0);
  if (!frame.has_value() || canPort_.send(*frame, session.responseTimeoutMs) != can::CanResult::Ok) {
    return elm::textReply(session, elm::kCanErrorText);
  }
  return elm::textReply(session, elm::kOkText);
}

void ElmApplication::syncMonitorCanMode() {
  const elm::ElmSession& session = engine_.session();
  can::CanConfig config;
  config.bitrate = can::Bitrate::Bitrate500k;
  if (auto protocol = elm::toObdCanProtocol(session.protocol)) {
    config.bitrate = can::obd::bitrateFor(*protocol);
  }
  bool wantListenOnly = session.monitorActive && session.silentMonitoringEnabled;
  config.mode = wantListenOnly ? can::ControllerMode::ListenOnly : can::ControllerMode::Normal;
  canPort_.configure(config);
}

elm::ElmReplyText ElmApplication::pollMonitor(can::Milliseconds now) {
  (void)now;
  elm::ElmReplyText out;
  const elm::ElmSession& session = engine_.session();
  if (!session.monitorActive) {
    return out;
  }

  can::ReceiveResult rx = canPort_.receive();
  if (!rx.hasFrame) {
    return out;
  }

  bool matches = false;
  switch (session.monitorMode) {
    case elm::MonitorMode::All:
      matches = true;
      break;
    case elm::MonitorMode::ReceivedAddress:
    case elm::MonitorMode::TransmittedAddress:
      matches = (rx.frame.id & 0xFF) == session.monitorAddressByte;
      break;
    default:
      break;
  }
  if (!matches) {
    return out;
  }

  out = elm::formatResponseBody(session, rx.frame, rx.frame.data.data(), rx.frame.dlc,
                                 rx.frame.dlc);
  out += elm::responseEnding(session);
  return out;
}

void ElmApplication::stopMonitor() {
  engine_.session().monitorActive = false;
  engine_.session().monitorMode = elm::MonitorMode::None;
  syncMonitorCanMode();
}

}  // namespace esp_obd::app
