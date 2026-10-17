#include "elm/at_commands_monitoring.h"

#include <cstring>

#include "elm/elm_errors.h"
#include "elm/elm_formatter.h"
#include "elm/elm_parser.h"

namespace esp_obd::elm {

namespace {

bool eq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

// ATMA/ATMR/ATMT "do not immediately produce a response ending or prompt"
// (docs/ELM_COMMAND_BEHAVIOR.md section 1.2).
ElmReply silentEntry() {
  ElmReply reply;
  reply.kind = ElmReplyKind::Text;
  reply.appendPrompt = false;
  return reply;
}

}  // namespace

std::optional<ElmReply> dispatchMonitoringCommand(ElmSession& session, const char* r) {
  if (eq(r, "MA")) {
    session.monitorActive = true;
    session.monitorMode = MonitorMode::All;
    return silentEntry();
  }
  if (std::strncmp(r, "MR", 2) == 0) {
    auto value = parseFixedWidthHex(r + 2, 2);
    if (!value.has_value()) return std::nullopt;
    session.monitorActive = true;
    session.monitorMode = MonitorMode::ReceivedAddress;
    session.monitorAddressByte = static_cast<uint8_t>(*value);
    return silentEntry();
  }
  if (std::strncmp(r, "MT", 2) == 0) {
    auto value = parseFixedWidthHex(r + 2, 2);
    if (!value.has_value()) return std::nullopt;
    session.monitorActive = true;
    session.monitorMode = MonitorMode::TransmittedAddress;
    session.monitorAddressByte = static_cast<uint8_t>(*value);
    return silentEntry();
  }

  if (eq(r, "CS")) {
    ElmReply reply;
    reply.kind = ElmReplyKind::CanStatusRequest;
    return reply;
  }

  if (eq(r, "CSM0")) {
    session.silentMonitoringEnabled = false;
    return textReply(session, kOkText);
  }
  if (eq(r, "CSM1")) {
    session.silentMonitoringEnabled = true;
    return textReply(session, kOkText);
  }

  if (eq(r, "RTR")) {
    ElmReply reply;
    reply.kind = ElmReplyKind::SendRtrRequest;
    return reply;
  }

  if (eq(r, "V0")) {
    session.variableDlcEnabled = false;
    return textReply(session, kOkText);
  }
  if (eq(r, "V1")) {
    session.variableDlcEnabled = true;
    return textReply(session, kOkText);
  }

  if (eq(r, "BD")) {
    if (!session.lastTransmittedFrame.has_value() &&
        !session.lastAcceptedReceivedFrame.has_value()) {
      return textReply(session, kNoDataText);
    }
    // Raw dump of the last request frame sent, then the last frame
    // accepted, each as "ID DLC bytes" regardless of ATH/ATD.
    ElmSession dumpSession = session;
    dumpSession.headersEnabled = true;
    dumpSession.displayDlcEnabled = true;
    ElmReplyText body;
    for (const auto* frame : {&session.lastTransmittedFrame, &session.lastAcceptedReceivedFrame}) {
      if (!frame->has_value()) continue;
      if (!body.empty()) body += lineBreak(session);
      const can::CanFrame& f = **frame;
      body += formatResponseBody(dumpSession, f, f.data.data(), f.dlc, f.dlc).c_str();
    }
    return textReply(session, body.c_str());
  }

  return std::nullopt;
}

}  // namespace esp_obd::elm
