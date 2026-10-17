#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "core/fixed_string.h"

// Input normalization, AT-vs-hex classification, and strict hex-request
// validation -- see docs/ARCHITECTURE.md and
// docs/ELM_COMMAND_BEHAVIOR.md sections 1.1 and 2.5.

namespace esp_obd::elm {

inline constexpr size_t kNormalizedLineCapacity = 256;
using NormalizedLine = FixedString<kNormalizedLineCapacity>;

// Uppercases and strips ASCII spaces. Does not interpret CR/LF: the caller
// (eventually a LineReader, T08) already delivers one complete line.
NormalizedLine normalizeLine(const char* rawLine);

inline bool startsWithAt(const NormalizedLine& line) {
  return line.size() >= 2 && line.c_str()[0] == 'A' && line.c_str()[1] == 'T';
}

struct HexRequestValidation {
  bool valid = false;
  // Pure payload hex text (even length, trailing response-count nibble
  // already stripped if present). Byte-decoding is left to the diagnostic
  // layer (T06), which needs the bytes anyway and shouldn't duplicate this
  // parse.
  NormalizedLine payloadHex;
  std::optional<uint8_t> maxResponses;
};

// Validates a non-AT, non-empty normalized line per
// docs/ELM_COMMAND_BEHAVIOR.md section 2.5's rejection rules: odd length
// ending in '0', invalid hex, no payload, or (when automatic formatting is
// off, i.e. CAF0) a payload longer than 8 bytes are all rejected.
HexRequestValidation validateHexRequestLine(const NormalizedLine& line,
                                             bool automaticFormattingEnabled);

// Decodes an even-length hex-digit string (e.g. HexRequestValidation's
// payloadHex) into bytes, writing at most `maxOut` bytes. The diagnostic
// layer (whoever executes a DiagnosticRequest-kind ElmReply) is the one
// caller: decoding stays here so it isn't duplicated per caller.
size_t decodeHexBytes(const char* hexDigits, uint8_t* out, size_t maxOut);

// Parses exactly `digitCount` hex digits (1-8) followed by NUL -- e.g. an
// ATSThh argument (2) or an ATSHhhhhhhhh argument (8). Shared by the T09
// command families so each doesn't hand-roll its own hex/length checks.
std::optional<uint32_t> parseFixedWidthHex(const char* text, size_t digitCount);

}  // namespace esp_obd::elm
