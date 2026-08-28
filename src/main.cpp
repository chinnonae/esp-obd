#include <Arduino.h>

#include "app/elm_application.h"
#include "can/can_config.h"
#include "core/build_info.h"
#include "platform/esp32/esp32_bluetooth_transport.h"
#include "platform/esp32/esp32_clock.h"
#include "platform/esp32/esp32_settings_store.h"
#include "platform/esp32/esp32_twai_can_port.h"
#include "platform/esp32/esp32_uart_debug_sink.h"

namespace {

esp_obd::platform::esp32::Esp32TwaiCanPort gCanPort;
esp_obd::platform::esp32::Esp32Clock gClock;
esp_obd::platform::esp32::Esp32SettingsStore gSettingsStore;
esp_obd::app::ElmApplication gApp(gCanPort, gSettingsStore);
esp_obd::platform::esp32::Esp32BluetoothTransport gBluetooth(gApp);
esp_obd::platform::esp32::Esp32UartDebugSink gUartDebug;

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.print(esp_obd::build::kAdapterDescription);
  Serial.print(" v");
  Serial.println(esp_obd::build::kFirmwareVersion);

  esp_obd::can::CanConfig canConfig;
  canConfig.bitrate = esp_obd::can::Bitrate::Bitrate500k;
  canConfig.mode = esp_obd::can::ControllerMode::Normal;
  gCanPort.configure(canConfig);

  gBluetooth.begin();
}

void loop() {
  esp_obd::can::Milliseconds now = gClock.now();
  gBluetooth.poll(now);
  gUartDebug.poll(now);
}
