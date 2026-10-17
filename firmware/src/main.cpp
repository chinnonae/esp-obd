#include <Arduino.h>
#include <nvs_flash.h>

#include <optional>

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

// Constructed inside setup(), in this order, after nvs_flash_init(): each
// one's constructor (transitively, via ElmCommandEngine) reads gSettingsStore,
// so gSettingsStore.load() must run first. A file-scope object would
// construct during C++ static init, before setup() -- and before NVS is
// ready (confirmed against real hardware: Preferences::begin() failed with
// NOT_INITIALIZED when this was a plain file-scope global).
std::optional<esp_obd::app::ElmApplication> gApp;
std::optional<esp_obd::platform::esp32::Esp32BluetoothTransport> gBluetooth;
std::optional<esp_obd::platform::esp32::Esp32UartDebugSink> gUartDebug;

void initNvs() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    Serial.print("#DBG: nvs_flash_init failed: ");
    Serial.println(err);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.print(esp_obd::build::kAdapterDescription);
  Serial.print(" v");
  Serial.println(esp_obd::build::kFirmwareVersion);

  initNvs();
  gSettingsStore.load();

  esp_obd::can::CanConfig canConfig;
  canConfig.bitrate = esp_obd::can::Bitrate::Bitrate500k;
  canConfig.mode = esp_obd::can::ControllerMode::Normal;
  gCanPort.configure(canConfig);

  gApp.emplace(gCanPort, gSettingsStore);
  gBluetooth.emplace(*gApp);
  gBluetooth->begin();
  gUartDebug.emplace();

  Serial.println("#DBG: setup complete");
}

void loop() {
  esp_obd::can::Milliseconds now = gClock.now();
  gBluetooth->poll(now);
  gUartDebug->poll(now);
}
