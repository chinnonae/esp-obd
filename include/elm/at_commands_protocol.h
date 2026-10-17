#pragma once

#include <optional>

#include "elm/elm_reply.h"
#include "elm/elm_session.h"

// Handlers for ATSP/ATTP/ATDP/ATDPN/ATST/ATAT/ATCTM/ATPC
// (docs/ELM_COMMAND_BEHAVIOR.md section 2.2). CAN port reconfiguration
// (TWAI bitrate) never happens here: it happens lazily, right before the
// next diagnostic transaction or monitor session, in app/ElmApplication --
// this layer must not touch ICanPort.

namespace esp_obd::elm {

std::optional<ElmReply> dispatchProtocolCommand(ElmSession& session, const char* atRemainder);

}  // namespace esp_obd::elm
