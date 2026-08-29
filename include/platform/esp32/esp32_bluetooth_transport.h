#pragma once

#include "app/elm_bluetooth_session.h"

// Thin ESP32 adapter: shuttles real BluetoothSerial bytes through the
// portable ElmBluetoothSession and writes back whatever it returns. Owns
// no ELM/diagnostic logic itself -- see docs/ARCHITECTURE.md.

namespace esp_obd::platform::esp32 {

class Esp32BluetoothTransport {
 public:
  explicit Esp32BluetoothTransport(app::ElmApplication& app) : session_(app) {}

  void begin();
  void poll(can::Milliseconds now);

 private:
  app::ElmBluetoothSession session_;
  bool wasConnected_ = false;

  // Debug-log-only buffer: accumulates received bytes and logs them as one
  // "#DBG: BT> <line>" once a full command (terminated by \r) arrives,
  // instead of one log line per byte. Purely a UART0 debug convenience --
  // the real per-byte processing still goes through session_ unbuffered.
  static constexpr size_t kLogBufferCapacity = 80;
  char logBuffer_[kLogBufferCapacity + 1] = {};
  size_t logBufferLen_ = 0;
};

}  // namespace esp_obd::platform::esp32
