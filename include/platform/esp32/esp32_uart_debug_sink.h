#pragma once

#include "app/debug_console.h"
#include "app/line_reader.h"

// UART0-only debug console adapter: `#` commands in, `#DBG:`-prefixed
// lines out. Has no reference to any Bluetooth transport at all, so it
// structurally cannot write to it -- see docs/ARCHITECTURE.md.

namespace esp_obd::platform::esp32 {

class Esp32UartDebugSink {
 public:
  Esp32UartDebugSink();

  void poll(can::Milliseconds now);

 private:
  void handleCommand(const app::DebugCommand& command);
  void writeLine(const char* message);

  app::LineReader lineReader_;
  int debugLevel_ = 0;
};

}  // namespace esp_obd::platform::esp32
