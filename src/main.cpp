#include <Arduino.h>

#include "core/build_info.h"
#include "core/hardware_constants.h"

namespace {
constexpr uint32_t kBlinkDelayMs = 500;

void setLed(bool on) {
  const bool level = on == esp_obd::hw::kStatusLedActiveHigh;
  digitalWrite(esp_obd::hw::kStatusLedPin, level ? HIGH : LOW);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(esp_obd::hw::kStatusLedPin, OUTPUT);
  Serial.print(esp_obd::build::kAdapterDescription);
  Serial.print(" v");
  Serial.println(esp_obd::build::kFirmwareVersion);
  Serial.println("Hello, world!");
}

void loop() {
  setLed(true);
  delay(kBlinkDelayMs);
  setLed(false);
  delay(kBlinkDelayMs);
}
