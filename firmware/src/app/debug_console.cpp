#include "app/debug_console.h"

#include <cstring>

namespace esp_obd::app {

DebugCommand parseDebugCommand(const char* line) {
  DebugCommand command;

  if (std::strcmp(line, "#HELP") == 0) {
    command.kind = DebugCommandKind::Help;
    return command;
  }
  if (std::strcmp(line, "#STATUS") == 0) {
    command.kind = DebugCommandKind::Status;
    return command;
  }
  if (std::strcmp(line, "#REBOOT") == 0) {
    command.kind = DebugCommandKind::Reboot;
    return command;
  }
  if (std::strncmp(line, "#DBG ", 5) == 0) {
    const char* arg = line + 5;
    if (arg[0] >= '0' && arg[0] <= '3' && arg[1] == '\0') {
      command.kind = DebugCommandKind::SetDebugLevel;
      command.debugLevel = arg[0] - '0';
      return command;
    }
  }

  command.kind = DebugCommandKind::Unknown;
  return command;
}

DebugLine prefixDebugLine(const char* message) {
  DebugLine line;
  line = "#DBG: ";
  line += message;
  return line;
}

}  // namespace esp_obd::app
