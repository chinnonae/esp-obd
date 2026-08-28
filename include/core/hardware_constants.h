#pragma once

#include <cstdint>

// Confirmed physical wiring and identity for the `ioxesp32` target only.
// Do not add a pin or setting here unless it has been confirmed against real
// hardware; record the confirmation in docs/tasks/00-project-baseline.md.

namespace esp_obd::hw {

// CAN transceiver connection (Classical CAN only, via ESP32 TWAI).
inline constexpr uint8_t kCanTxPin = 26;
inline constexpr uint8_t kCanRxPin = 27;

// Board's built-in LED (`LED_BUILTIN` in the ioxesp32 pins_arduino.h variant).
inline constexpr uint8_t kStatusLedPin = 5;
inline constexpr bool kStatusLedActiveHigh = true;

// Bluetooth Classic SPP identity used for the ELM327 command channel.
inline constexpr char kBluetoothDeviceName[] = "ESP-OBD";
inline constexpr char kBluetoothPairingPin[] = "5678";

static_assert(kCanTxPin != kCanRxPin, "CAN TX/RX pins must be distinct");
static_assert(kCanTxPin < 34,
              "GPIO34-39 are input-only on ESP32 and cannot drive TWAI TX");
static_assert(kCanRxPin <= 39, "GPIO out of ESP32 range");

}  // namespace esp_obd::hw
