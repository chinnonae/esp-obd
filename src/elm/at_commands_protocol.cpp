#include "elm/at_commands_protocol.h"

#include <cstring>

#include "can/obd_addresses.h"
#include "elm/elm_errors.h"
#include "elm/elm_formatter.h"
#include "elm/elm_parser.h"
#include "elm/protocol_mapping.h"

namespace esp_obd::elm {

namespace {

bool eq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

// "6".."9", or "" if not a concrete protocol (AutomaticSearch).
const char* protocolNumberText(ElmProtocol protocol) {
  switch (protocol) {
    case ElmProtocol::Iso15765_11bit_500k:
      return "6";
    case ElmProtocol::Iso15765_29bit_500k:
      return "7";
    case ElmProtocol::Iso15765_11bit_250k:
      return "8";
    case ElmProtocol::Iso15765_29bit_250k:
      return "9";
    default:
      return "";
  }
}

const char* protocolDescriptionText(ElmProtocol protocol) {
  switch (protocol) {
    case ElmProtocol::Iso15765_11bit_500k:
      return "ISO 15765-4 (CAN 11/500)";
    case ElmProtocol::Iso15765_29bit_500k:
      return "ISO 15765-4 (CAN 29/500)";
    case ElmProtocol::Iso15765_11bit_250k:
      return "ISO 15765-4 (CAN 11/250)";
    case ElmProtocol::Iso15765_29bit_250k:
      return "ISO 15765-4 (CAN 29/250)";
    default:
      return "AUTO";
  }
}

std::optional<ElmReply> selectProtocol(ElmSession& session, ElmProtocol protocol) {
  session.protocol = protocol;
  session.protocolConnected = true;
  session.protocolDiscoveredViaAutoSearch = false;
  session.requestId = can::obd::functionalRequestId(*toObdCanProtocol(protocol));
  return textReply(session, kOkText);
}

}  // namespace

std::optional<ElmReply> dispatchProtocolCommand(ElmSession& session, const char* r) {
  if (eq(r, "SP0")) {
    session.protocol = ElmProtocol::AutomaticSearch;
    session.protocolConnected = false;
    session.protocolDiscoveredViaAutoSearch = false;
    session.requestId = 0x7DF;
    return textReply(session, kOkText);
  }
  if (eq(r, "SP6")) return selectProtocol(session, ElmProtocol::Iso15765_11bit_500k);
  if (eq(r, "SP7")) return selectProtocol(session, ElmProtocol::Iso15765_29bit_500k);
  if (eq(r, "SP8")) return selectProtocol(session, ElmProtocol::Iso15765_11bit_250k);
  if (eq(r, "SP9")) return selectProtocol(session, ElmProtocol::Iso15765_29bit_250k);

  // ATTPx / ATTPAx: same selection semantics as ATSP for the in-scope
  // protocols (docs/ELM_COMMAND_BEHAVIOR.md section 2.2). The 'A' variant's
  // "optionally falling back to auto search" nuance isn't separately
  // modeled: both just select the concrete protocol immediately.
  if (eq(r, "TP6") || eq(r, "TPA6")) return selectProtocol(session, ElmProtocol::Iso15765_11bit_500k);
  if (eq(r, "TP7") || eq(r, "TPA7")) return selectProtocol(session, ElmProtocol::Iso15765_29bit_500k);
  if (eq(r, "TP8") || eq(r, "TPA8")) return selectProtocol(session, ElmProtocol::Iso15765_11bit_250k);
  if (eq(r, "TP9") || eq(r, "TPA9")) return selectProtocol(session, ElmProtocol::Iso15765_29bit_250k);

  if (eq(r, "DP")) {
    ElmReplyText body;
    if (session.protocolDiscoveredViaAutoSearch) {
      body += "AUTO, ";
    }
    body += protocolDescriptionText(session.protocol);
    return textReply(session, body.c_str());
  }

  if (eq(r, "DPN")) {
    if (session.protocol == ElmProtocol::AutomaticSearch && !session.protocolConnected) {
      return textReply(session, "0");
    }
    ElmReplyText body;
    if (session.protocolDiscoveredViaAutoSearch) {
      body += "A";
    }
    body += protocolNumberText(session.protocol);
    return textReply(session, body.c_str());
  }

  if (std::strncmp(r, "ST", 2) == 0) {
    auto value = parseFixedWidthHex(r + 2, 2);
    if (!value.has_value()) return std::nullopt;
    can::Milliseconds timeout = static_cast<can::Milliseconds>(*value) * 4;
    session.responseTimeoutMs = timeout < 4 ? 4 : timeout;
    return textReply(session, kOkText);
  }

  if (eq(r, "AT0")) { session.adaptiveTiming = AdaptiveTiming::Off; return textReply(session, kOkText); }
  if (eq(r, "AT1")) { session.adaptiveTiming = AdaptiveTiming::Mode1; return textReply(session, kOkText); }
  if (eq(r, "AT2")) { session.adaptiveTiming = AdaptiveTiming::Mode2; return textReply(session, kOkText); }

  if (eq(r, "CTM1")) { session.canTimeoutMultiplier = 1; return textReply(session, kOkText); }
  if (eq(r, "CTM5")) { session.canTimeoutMultiplier = 5; return textReply(session, kOkText); }

  if (eq(r, "PC")) {
    session.protocolConnected = false;
    session.monitorActive = false;
    session.monitorMode = MonitorMode::None;
    return textReply(session, kOkText);
  }

  return std::nullopt;
}

}  // namespace esp_obd::elm
