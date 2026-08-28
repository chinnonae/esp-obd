#pragma once

// Maps a typed failure to its exact ELM reply text. Keep every reply
// spelling here, not scattered across handler files -- see
// docs/ARCHITECTURE.md.
//
// Only the malformed/unknown/unsupported-command case is in scope for T03.
// T06/T09 will add NO DATA, CAN ERROR, and UNABLE TO CONNECT here once a
// DiagnosticTransport exists to produce those typed results.

namespace esp_obd::elm {

inline constexpr const char* kUnknownCommandText = "?";
inline constexpr const char* kOkText = "OK";

}  // namespace esp_obd::elm
