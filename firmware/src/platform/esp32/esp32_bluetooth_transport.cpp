#include "platform/esp32/esp32_bluetooth_transport.h"

#include <BluetoothSerial.h>

#include "core/hardware_constants.h"

namespace esp_obd::platform::esp32 {

namespace {
// One physical Bluetooth radio: a single instance is the natural shape,
// matching the Arduino BluetoothSerial library's own singleton-ish API.
BluetoothSerial gSerialBt;

// Debug-log one piece of ELM traffic with control characters made visible.
// The ELM protocol's own \r (and \r\n) bytes are real reply/echo content,
// but printed raw to a live terminal they move the cursor without a
// newline, so each line silently overwrites the last -- this makes a
// multi-responder reply look like it never arrived. Escaping them here is
// UART0-debug-only; it never touches what's written back over Bluetooth.
void logDebugTraffic(const char* prefix, const char* text) {
  Serial.print(prefix);
  for (const char* p = text; *p != '\0'; ++p) {
    if (*p == '\r') {
      Serial.print("\\r");
    } else if (*p == '\n') {
      Serial.print("\\n");
    } else {
      Serial.print(*p);
    }
  }
  Serial.println();
}
}  // namespace

void Esp32BluetoothTransport::begin() {
  // begin() must come first: setPin() checks isReady() internally, which
  // fails ("BT is not initialized") if the stack isn't started yet --
  // confirmed against real hardware serial output.
  gSerialBt.begin(hw::kBluetoothDeviceName);
  gSerialBt.setPin(hw::kBluetoothPairingPin);
}

void Esp32BluetoothTransport::poll(can::Milliseconds now) {
  bool connected = gSerialBt.hasClient();
  if (connected != wasConnected_) {
    Serial.println(connected ? "#DBG: BT client connected" : "#DBG: BT client disconnected");
    if (wasConnected_ && !connected) {
      session_.onDisconnected();
    }
  }
  wasConnected_ = connected;

  while (gSerialBt.available() > 0) {
    int byte = gSerialBt.read();
    if (byte < 0) {
      break;
    }

    // Buffer for logging only: flush as one line once a full command
    // arrives (\r), rather than one log line per byte.
    if (byte == '\r') {
      logBuffer_[logBufferLen_] = '\0';
      logDebugTraffic("#DBG: BT> ", logBuffer_);
      logBufferLen_ = 0;
    } else if (logBufferLen_ < kLogBufferCapacity) {
      logBuffer_[logBufferLen_++] = static_cast<char>(byte);
    }

    app::TransportOutput out = session_.onByte(now, static_cast<uint8_t>(byte));
    if (!out.empty()) {
      logDebugTraffic("#DBG: BT< ", out.c_str());
      gSerialBt.write(reinterpret_cast<const uint8_t*>(out.c_str()), out.size());
    }
  }

  app::TransportOutput polled = session_.poll(now);
  if (!polled.empty()) {
    logDebugTraffic("#DBG: BT< ", polled.c_str());
    gSerialBt.write(reinterpret_cast<const uint8_t*>(polled.c_str()), polled.size());
  }
}

}  // namespace esp_obd::platform::esp32
