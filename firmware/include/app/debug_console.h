#pragma once

#include "core/fixed_string.h"

// Portable UART `#` command parsing and output-line prefixing -- see
// docs/ARCHITECTURE.md's DebugConsole. This has no path to a Bluetooth
// transport at all, and never touches the ELM parser: matching against
// exact "#..." literals (case-sensitive, unlike ELM commands) makes it
// structurally impossible for a UART command to be parsed as an ELM one.

namespace esp_obd::app {

enum class DebugCommandKind {
  Help,
  Status,
  SetDebugLevel,  // debugLevel is 0..3
  Reboot,
  Unknown,
};

struct DebugCommand {
  DebugCommandKind kind = DebugCommandKind::Unknown;
  int debugLevel = 0;  // meaningful only for SetDebugLevel
};

// Parses one already-assembled line (see LineReader). Case-sensitive,
// exact match; "#DBG " must be followed by exactly one digit 0-3.
DebugCommand parseDebugCommand(const char* line);

inline constexpr size_t kDebugLineCapacity = 128;
using DebugLine = FixedString<kDebugLineCapacity>;

// Prefixes every UART debug line so it is never mistaken for ELM/
// Bluetooth output, per docs/tasks/08-transports-and-debug.md.
DebugLine prefixDebugLine(const char* message);

}  // namespace esp_obd::app
