#pragma once

#include <optional>

#include "elm/elm_reply.h"
#include "elm/elm_session.h"

// Handlers for ATSH/ATCP/ATCRA/ATAR/ATCF/ATCM/ATCAF/ATCFC/ATFCSM/ATFCSH/
// ATFCSD/ATCEA/ATCEAhh/ATCERhh (docs/ELM_COMMAND_BEHAVIOR.md section 2.3).
// Pure session mutation; never touches ICanPort.

namespace esp_obd::elm {

std::optional<ElmReply> dispatchAddressingCommand(ElmSession& session, const char* atRemainder);

}  // namespace esp_obd::elm
