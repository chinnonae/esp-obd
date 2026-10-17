#include "platform/esp32/esp32_uart_debug_sink.h"

#include <Arduino.h>

#include <cstdio>

namespace esp_obd::platform::esp32 {

namespace {
constexpr can::Milliseconds kUartIdleTimeoutMs = 5000;
}  // namespace

Esp32UartDebugSink::Esp32UartDebugSink() : lineReader_(kUartIdleTimeoutMs) {}

void Esp32UartDebugSink::writeLine(const char* message) {
  app::DebugLine line = app::prefixDebugLine(message);
  Serial.println(line.c_str());
}

void Esp32UartDebugSink::handleCommand(const app::DebugCommand& command) {
  switch (command.kind) {
    case app::DebugCommandKind::Help:
      writeLine("commands: #HELP #STATUS #REBOOT #DBG <0-3>");
      writeLine("levels: 0=off 1=info 2=CAN frames 3=ELM/ISO-TP trace");
      writeLine("ELM327 is on Bluetooth SPP only; UART0 is debug");
      break;

    case app::DebugCommandKind::Status: {
      char buf[96];
      std::snprintf(buf, sizeof(buf), "debugLevel=%d uptimeMs=%lu", debugLevel_,
                    static_cast<unsigned long>(millis()));
      writeLine(buf);
      break;
    }

    case app::DebugCommandKind::SetDebugLevel: {
      debugLevel_ = command.debugLevel;
      char buf[24];
      std::snprintf(buf, sizeof(buf), "debug level=%d", debugLevel_);
      writeLine(buf);
      break;
    }

    case app::DebugCommandKind::Reboot:
      writeLine("rebooting");
      delay(50);  // one-shot, intentional: let the serial write flush before reset
      ESP.restart();
      break;

    case app::DebugCommandKind::Unknown:
      writeLine("unknown debug command");
      break;
  }
}

void Esp32UartDebugSink::poll(can::Milliseconds now) {
  while (Serial.available() > 0) {
    int byte = Serial.read();
    if (byte < 0) {
      break;
    }
    app::LineEvent event = lineReader_.onByte(now, static_cast<uint8_t>(byte));
    if (event.kind == app::LineEventKind::Line) {
      handleCommand(app::parseDebugCommand(event.text.c_str()));
    }
    // Overflow/TimedOut: no response needed for a debug-only console.
  }
}

}  // namespace esp_obd::platform::esp32
