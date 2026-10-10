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

// "\r" (ATL0) or "\r\n" (ATL1): what ends each line of a multi-line
// response. The final line is followed by one more, via responseEnding()'s
// doubled form, before the prompt -- e.g. "41 00 ..\r41 00 ..\r\r>".
const char* lineBreak(const ElmSession& session);

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

// Headers-off (ATH0, CAF1) rendering of a multi-frame ISO-TP response, as an
// ELM327 prints it: the payload length as three hex digits, then the payload
// split the way it travelled on the bus, each line prefixed with its frame
// index 0-F (wrapping):
//   014
//   0: 49 02 01 31 48 47
//   1: 43 4D 38 32 36 33 33
// `firstFrameBytes`/`consecutiveFrameBytes` are the payload bytes each frame
// carried (6/7, or 5/6 with ISO-TP extended addressing). Lines are joined
// with lineBreak(); there is no trailing line break. The final frame's
// padding is not printed -- the length line already marks the end of data.
ElmReplyText formatMultiFrameBody(const ElmSession& session, const uint8_t* payload,
                                   size_t payloadLen, size_t firstFrameBytes,
                                   size_t consecutiveFrameBytes);

}  // namespace esp_obd::elm
