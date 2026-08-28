#include "platform/esp32/esp32_bluetooth_transport.h"

#include <BluetoothSerial.h>

#include "core/hardware_constants.h"

namespace esp_obd::platform::esp32 {

namespace {
// One physical Bluetooth radio: a single instance is the natural shape,
// matching the Arduino BluetoothSerial library's own singleton-ish API.
BluetoothSerial gSerialBt;
}  // namespace

void Esp32BluetoothTransport::begin() {
  gSerialBt.setPin(hw::kBluetoothPairingPin);
  gSerialBt.begin(hw::kBluetoothDeviceName);
}

void Esp32BluetoothTransport::poll(can::Milliseconds now) {
  bool connected = gSerialBt.hasClient();
  if (wasConnected_ && !connected) {
    session_.onDisconnected();
  }
  wasConnected_ = connected;

  while (gSerialBt.available() > 0) {
    int byte = gSerialBt.read();
    if (byte < 0) {
      break;
    }
    app::TransportOutput out = session_.onByte(now, static_cast<uint8_t>(byte));
    if (!out.empty()) {
      gSerialBt.write(reinterpret_cast<const uint8_t*>(out.c_str()), out.size());
    }
  }

  app::TransportOutput polled = session_.poll(now);
  if (!polled.empty()) {
    gSerialBt.write(reinterpret_cast<const uint8_t*>(polled.c_str()), polled.size());
  }
}

}  // namespace esp_obd::platform::esp32
