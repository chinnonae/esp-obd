#pragma once

#include <cstddef>
#include <cstdint>

#include "can/can_frame.h"
#include "elm/elm_reply.h"
#include "elm/elm_session.h"

// Response endings, hex/byte formatting, headers, spacing, and prompt
// decisions all live here -- see docs/ARCHITECTURE.md. This layer never
// interprets ISO-TP PCI bytes: it renders whatever raw frame and already
// -reassembled payload it is given (that reassembly is T04/T06's job).

namespace esp_obd::elm {

// "\r\r" (ATL0, default) or "\r\n\r\n" (ATL1). Per
// docs/ELM_COMMAND_BEHAVIOR.md section 1.2, "OK uses the *new* line-ending
// mode": callers must mutate session.linefeedsEnabled before formatting.
const char* responseEnding(const ElmSession& session);

// Wraps `body` with the current response ending into a complete Text reply.
ElmReply textReply(const ElmSession& session, const char* body);

// Appends two uppercase hex digits for `value`. Shared by response-body
// rendering and any handler needing the same "one byte as hex" spelling
// (e.g. T11's ATRD), so it isn't duplicated per file.
void appendHexByte(ElmReplyText& out, uint8_t value);

// Renders one response line's body (no response ending):
// - headers off: prints `payload[0..payloadLen)` (the CAF1-reassembled
//   ISO-TP payload, or the raw CAF0 bytes -- the caller decides which by
//   what it passes as `payload`).
// - headers on: prints the CAN id (+ DLC if session.displayDlcEnabled),
//   then `rawFrame.data[0..rawBytesToShow)`. `rawBytesToShow` may be less
//   than `rawFrame.dlc`: with ATD0, automatic-formatting trims CAN-level
//   padding even in headers-on mode (see the contract's worked example);
//   with ATD1 it equals `rawFrame.dlc`. The caller (T04/T06) computes this
//   from the ISO-TP PCI, not the formatter.
ElmReplyText formatResponseBody(const ElmSession& session, const can::CanFrame& rawFrame,
                                 const uint8_t* payload, size_t payloadLen,
                                 size_t rawBytesToShow);

}  // namespace esp_obd::elm
