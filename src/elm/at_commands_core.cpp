#include "elm/at_commands_core.h"

#include <cstring>

#include "core/build_info.h"
#include "elm/elm_errors.h"
#include "elm/elm_formatter.h"

namespace esp_obd::elm {

namespace {
bool eq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
}  // namespace

std::optional<ElmReply> dispatchCoreCommand(ElmSession& session, const char* r) {
  if (eq(r, "Z")) {
    session.resetToDefaults();
    return textReply(session, esp_obd::build::kElmVersion);
  }
  if (eq(r, "WS")) {
    session.resetToDefaults();
    return textReply(session, esp_obd::build::kElmVersion);
  }
  if (eq(r, "D")) {
    session.resetToDefaults();
    return textReply(session, kOkText);
  }
  if (eq(r, "I")) {
    return textReply(session, esp_obd::build::kElmVersion);
  }
  if (eq(r, "@1")) {
    return textReply(session, esp_obd::build::kAdapterDescription);
  }
  if (eq(r, "E0")) {
    session.echoEnabled = false;
    return textReply(session, kOkText);
  }
  if (eq(r, "E1")) {
    session.echoEnabled = true;
    return textReply(session, kOkText);
  }
  if (eq(r, "L0")) {
    session.linefeedsEnabled = false;
    return textReply(session, kOkText);
  }
  if (eq(r, "L1")) {
    session.linefeedsEnabled = true;
    return textReply(session, kOkText);
  }
  if (eq(r, "S0")) {
    session.spacesEnabled = false;
    return textReply(session, kOkText);
  }
  if (eq(r, "S1")) {
    session.spacesEnabled = true;
    return textReply(session, kOkText);
  }
  if (eq(r, "H0")) {
    session.headersEnabled = false;
    return textReply(session, kOkText);
  }
  if (eq(r, "H1")) {
    session.headersEnabled = true;
    return textReply(session, kOkText);
  }
  if (eq(r, "D0")) {
    session.displayDlcEnabled = false;
    return textReply(session, kOkText);
  }
  if (eq(r, "D1")) {
    session.displayDlcEnabled = true;
    return textReply(session, kOkText);
  }
  if (eq(r, "R0")) {
    session.responsesEnabled = false;
    return textReply(session, kOkText);
  }
  if (eq(r, "R1")) {
    session.responsesEnabled = true;
    return textReply(session, kOkText);
  }

  return std::nullopt;
}

}  // namespace esp_obd::elm
