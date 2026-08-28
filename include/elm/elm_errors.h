#pragma once

// Maps a typed failure to its exact ELM reply text. Keep every reply
// spelling here, not scattered across handler files -- see
// docs/ARCHITECTURE.md.

namespace esp_obd::elm {

inline constexpr const char* kUnknownCommandText = "?";
inline constexpr const char* kOkText = "OK";

// Added once DiagnosticTransport (T06) had a real caller to produce these
// typed outcomes (T08's ElmApplication).
inline constexpr const char* kNoDataText = "NO DATA";
inline constexpr const char* kCanErrorText = "CAN ERROR";
inline constexpr const char* kUnableToConnectText = "UNABLE TO CONNECT";

// Monitor mode (T09 will add the ATMA/ATMR/ATMT commands that start it;
// T08 wires the "any byte stops it" side since ElmSession.monitorActive
// already exists).
inline constexpr const char* kStoppedText = "STOPPED";

}  // namespace esp_obd::elm
