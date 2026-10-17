#include "platform/esp32/esp32_clock.h"

#include <Arduino.h>

namespace esp_obd::platform::esp32 {

can::Milliseconds Esp32Clock::now() const { return static_cast<can::Milliseconds>(millis()); }

}  // namespace esp_obd::platform::esp32
