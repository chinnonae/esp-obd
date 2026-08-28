#include "platform/esp32/esp32_twai_can_port.h"

#include <driver/gpio.h>
#include <driver/twai.h>
#include <freertos/FreeRTOS.h>

#include "core/hardware_constants.h"

namespace esp_obd::platform::esp32 {

namespace {

twai_timing_config_t timingFor(can::Bitrate bitrate) {
  switch (bitrate) {
    case can::Bitrate::Bitrate500k: {
      twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
      return timing;
    }
    case can::Bitrate::Bitrate250k:
    default: {
      twai_timing_config_t timing = TWAI_TIMING_CONFIG_250KBITS();
      return timing;
    }
  }
}

}  // namespace

bool Esp32TwaiCanPort::configure(const can::CanConfig& config) {
  if (installed_) {
    twai_stop();
    twai_driver_uninstall();
    installed_ = false;
  }

  twai_mode_t mode =
      config.mode == can::ControllerMode::ListenOnly ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL;
  twai_general_config_t generalConfig = TWAI_GENERAL_CONFIG_DEFAULT(
      static_cast<gpio_num_t>(hw::kCanTxPin), static_cast<gpio_num_t>(hw::kCanRxPin), mode);
  twai_timing_config_t timingConfig = timingFor(config.bitrate);
  twai_filter_config_t filterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&generalConfig, &timingConfig, &filterConfig) != ESP_OK) {
    return false;
  }
  if (twai_start() != ESP_OK) {
    twai_driver_uninstall();
    return false;
  }

  installed_ = true;
  currentConfig_ = config;
  return true;
}

can::CanResult Esp32TwaiCanPort::send(const can::CanFrame& frame, can::Milliseconds timeout) {
  if (!installed_) {
    return can::CanResult::BusError;
  }

  twai_message_t message = {};
  message.identifier = frame.id;
  message.extd = frame.extended ? 1 : 0;
  message.rtr = frame.remoteRequest ? 1 : 0;
  message.data_length_code = frame.dlc;
  for (uint8_t i = 0; i < frame.dlc && i < TWAI_FRAME_MAX_DLC; ++i) {
    message.data[i] = frame.data[i];
  }

  esp_err_t err = twai_transmit(&message, pdMS_TO_TICKS(timeout));
  if (err == ESP_OK) {
    return can::CanResult::Ok;
  }
  if (err == ESP_ERR_TIMEOUT) {
    return can::CanResult::Timeout;
  }
  return can::CanResult::BusError;
}

can::ReceiveResult Esp32TwaiCanPort::receive() {
  if (!installed_) {
    return can::ReceiveResult{/*hasFrame=*/false, {}};
  }

  twai_message_t message;
  // Always non-blocking (ticks_to_wait = 0), per the ICanPort contract in
  // docs/ARCHITECTURE.md.
  if (twai_receive(&message, 0) != ESP_OK) {
    return can::ReceiveResult{/*hasFrame=*/false, {}};
  }

  can::CanFrame frame;
  frame.id = message.identifier;
  frame.extended = message.extd != 0;
  frame.remoteRequest = message.rtr != 0;
  frame.dlc = message.data_length_code;
  for (uint8_t i = 0; i < frame.dlc && i < TWAI_FRAME_MAX_DLC; ++i) {
    frame.data[i] = message.data[i];
  }
  return can::ReceiveResult{/*hasFrame=*/true, frame};
}

can::CanStatus Esp32TwaiCanPort::status() const {
  can::CanStatus result;
  result.configuredBitrate = currentConfig_.bitrate;
  if (!installed_) {
    return result;
  }

  twai_status_info_t info;
  if (twai_get_status_info(&info) == ESP_OK) {
    result.txErrorCounter = info.tx_error_counter;
    result.rxErrorCounter = info.rx_error_counter;
    result.busOff = (info.state == TWAI_STATE_BUS_OFF);
  }
  return result;
}

}  // namespace esp_obd::platform::esp32
