#pragma once

#include <optional>

#include "elm/elm_reply.h"
#include "elm/elm_session.h"

// Handlers for the core session command family
// (docs/ELM_COMMAND_BEHAVIOR.md section 2.1, minus AT@2/AT@3/ATM/ATFE/
// ATRD/ATSD which T11 owns). A handler may mutate `session` or return an
// action; it never touches ICanPort.

namespace esp_obd::elm {

// Dispatches `atRemainder` (the normalized command text after "AT") to a
// core-family handler. Returns std::nullopt if this is not a command T03
// recognizes -- the caller then treats it as unknown, exactly as it would a
// genuinely unsupported command (both cases return `?` identically today;
// only which commands later tasks add to their own dispatch differs).
std::optional<ElmReply> dispatchCoreCommand(ElmSession& session, const char* atRemainder);

}  // namespace esp_obd::elm
