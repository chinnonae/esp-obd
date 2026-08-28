#include "app/elm_application.h"

#include "can/obd_addresses.h"
#include "elm/elm_errors.h"
#include "elm/elm_formatter.h"
#include "elm/elm_parser.h"

namespace esp_obd::app {

namespace {

// Reconciles elm::ElmProtocol (T03: 5 values, includes "AutomaticSearch"
// session state) with can::obd::ObdCanProtocol (T06: 4 wire configs, no
// "automatic" member -- diagnostic/ can't depend on elm/). Left here
// rather than as a permanent fixture: a future task may fold one into the
// other once ATSP (T09) shows which shape it actually wants.
std::optional<can::obd::ObdCanProtocol> toObdCanProtocol(elm::ElmProtocol protocol) {
  switch (protocol) {
    case elm::ElmProtocol::Iso15765_11bit_500k:
      return can::obd::ObdCanProtocol::Iso15765_11bit_500k;
    case elm::ElmProtocol::Iso15765_29bit_500k:
      return can::obd::ObdCanProtocol::Iso15765_29bit_500k;
    case elm::ElmProtocol::Iso15765_11bit_250k:
      return can::obd::ObdCanProtocol::Iso15765_11bit_250k;
    case elm::ElmProtocol::Iso15765_29bit_250k:
      return can::obd::ObdCanProtocol::Iso15765_29bit_250k;
    default:
      return std::nullopt;
  }
}

elm::ElmProtocol fromObdCanProtocol(can::obd::ObdCanProtocol protocol) {
  switch (protocol) {
    case can::obd::ObdCanProtocol::Iso15765_11bit_500k:
      return elm::ElmProtocol::Iso15765_11bit_500k;
    case can::obd::ObdCanProtocol::Iso15765_29bit_500k:
      return elm::ElmProtocol::Iso15765_29bit_500k;
    case can::obd::ObdCanProtocol::Iso15765_11bit_250k:
      return elm::ElmProtocol::Iso15765_11bit_250k;
    case can::obd::ObdCanProtocol::Iso15765_29bit_250k:
    default:
      return elm::ElmProtocol::Iso15765_29bit_250k;
  }
}

}  // namespace

elm::ElmReply ElmApplication::execute(can::Milliseconds now, const char* rawLine) {
  elm::ElmReply reply = engine_.execute(rawLine);
  if (reply.kind == elm::ElmReplyKind::DiagnosticRequest) {
    startDiagnostic(now, reply);
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
  // T09 will add a distinct ATCERhh field; until then this reuses the
  // single extended-address byte for both directions.
  request.requiredExtendedAddressByte = session.extendedAddressByte;
  request.sendAutomaticFlowControl = session.automaticFlowControlEnabled;
  request.responseTimeoutMs = session.responseTimeoutMs;

  diagnosticPending_ = true;

  auto fixedProtocol = toObdCanProtocol(session.protocol);
  if (!session.protocolConnected || !fixedProtocol.has_value()) {
    diagnosticTransport_.startAutoSearch(now, request);
  } else {
    request.requestId =
        session.customHeaderId.value_or(can::obd::functionalRequestId(*fixedProtocol));
    request.requestIdIsExtendedCan = can::obd::isExtendedCan(*fixedProtocol);
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
    // T09's ATDPN will need to separately track "discovered via search"
    // (for its A6..A9 vs 6..9 distinction) -- not modeled here yet.
    engine_.session().protocol = fromObdCanProtocol(*result.connectedProtocol);
    engine_.session().protocolConnected = true;
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
  // raw CF, PCI-declared-length trimming under ATD0) is left to T09.
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

}  // namespace esp_obd::app
