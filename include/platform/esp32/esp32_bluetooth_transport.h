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
};

}  // namespace esp_obd::platform::esp32
