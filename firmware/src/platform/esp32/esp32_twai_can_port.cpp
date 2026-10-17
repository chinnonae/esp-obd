#include "platform/esp32/esp32_twai_can_port.h"

#include <driver/gpio.h>
#include <driver/twai.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
  // send() waits for TX_SUCCESS to know a frame was actually acknowledged.
  generalConfig.alerts_enabled = TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_BUS_OFF;
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

bool Esp32TwaiCanPort::ensureRunning(can::Milliseconds timeout) {
  twai_status_info_t info;
  if (twai_get_status_info(&info) != ESP_OK) {
    return false;
  }
  if (info.state == TWAI_STATE_RUNNING) {
    return true;
  }
  if (info.state == TWAI_STATE_BUS_OFF && twai_initiate_recovery() != ESP_OK) {
    return false;
  }
  // Recovery ends in STOPPED after 128 x 11 recessive bits (about 3 ms at
  // 500 kbit/s); the controller then has to be started again.
  TickType_t start = xTaskGetTickCount();
  while (true) {
    if (twai_get_status_info(&info) != ESP_OK) {
      return false;
    }
    if (info.state == TWAI_STATE_STOPPED) {
      return twai_start() == ESP_OK;
    }
    if (info.state == TWAI_STATE_RUNNING) {
      return true;
    }
    if (xTaskGetTickCount() - start >= pdMS_TO_TICKS(timeout)) {
      return false;
    }
    vTaskDelay(1);
  }
}

void Esp32TwaiCanPort::restartController(can::Milliseconds timeout) {
  twai_status_info_t info;
  if (twai_get_status_info(&info) == ESP_OK && info.state == TWAI_STATE_RUNNING) {
    // Stopping drops the frame being retried and anything queued behind it.
    twai_stop();
    twai_start();
    return;
  }
  ensureRunning(timeout);
}

can::CanResult Esp32TwaiCanPort::send(const can::CanFrame& frame, can::Milliseconds timeout) {
  if (!installed_) {
    return can::CanResult::BusError;
  }
  // A controller that went bus-off (e.g. requests sent while no ECU was
  // awake to acknowledge them) stays off until recovered. Without this, one
  // quiet period would turn every later request into CAN ERROR.
  if (!ensureRunning(timeout)) {
    return can::CanResult::BusError;
  }
  // Listen-only mode never transmits, so there is no TX_SUCCESS to wait for.
  bool confirmTransmit = currentConfig_.mode == can::ControllerMode::Normal;
  uint32_t alerts = 0;
  twai_read_alerts(&alerts, 0);  // drop stale alerts from earlier frames

  twai_message_t message = {};
  message.identifier = frame.id;
  message.extd = frame.extended ? 1 : 0;
  message.rtr = frame.remoteRequest ? 1 : 0;
  message.data_length_code = frame.dlc;
  for (uint8_t i = 0; i < frame.dlc && i < TWAI_FRAME_MAX_DLC; ++i) {
    message.data[i] = frame.data[i];
  }

  TickType_t start = xTaskGetTickCount();
  esp_err_t err = twai_transmit(&message, pdMS_TO_TICKS(timeout));
  if (err == ESP_ERR_TIMEOUT) {
    restartController(timeout);
    return can::CanResult::Timeout;
  }
  if (err != ESP_OK) {
    return can::CanResult::BusError;
  }
  if (!confirmTransmit) {
    return can::CanResult::Ok;
  }

  // twai_transmit() only queues the frame. With nobody on the bus to ACK
  // it (car off), the controller would retry it forever and every later
  // frame would queue up behind it. Wait for the controller to report it
  // sent; otherwise abort it, so this request ends in CAN ERROR (as on an
  // ELM327) and the next one starts clean.
  while (true) {
    TickType_t elapsed = xTaskGetTickCount() - start;
    TickType_t limit = pdMS_TO_TICKS(timeout);
    TickType_t remaining = elapsed >= limit ? 0 : limit - elapsed;
    if (twai_read_alerts(&alerts, remaining) != ESP_OK) {
      restartController(timeout);
      return can::CanResult::Timeout;
    }
    if (alerts & TWAI_ALERT_TX_SUCCESS) {
      return can::CanResult::Ok;
    }
    if (alerts & TWAI_ALERT_BUS_OFF) {
      restartController(timeout);
      return can::CanResult::BusError;
    }
  }
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

void Esp32TwaiCanPort::discardPending() {
  if (installed_) {
    twai_clear_receive_queue();
  }
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
