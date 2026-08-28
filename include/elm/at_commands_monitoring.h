#pragma once

#include <optional>

#include "elm/elm_reply.h"
#include "elm/elm_session.h"

// Handlers for ATMA/ATMR/ATMT/ATCS/ATCSM/ATRTR/ATV/ATBD
// (docs/ELM_COMMAND_BEHAVIOR.md section 2.4). ATCS and ATRTR need live
// ICanPort access (a status read, a frame send) that this layer must not
// have directly: they return ElmReplyKind::CanStatusRequest/
// SendRtrRequest and app/ElmApplication performs the real action.

namespace esp_obd::elm {

std::optional<ElmReply> dispatchMonitoringCommand(ElmSession& session, const char* atRemainder);

}  // namespace esp_obd::elm
