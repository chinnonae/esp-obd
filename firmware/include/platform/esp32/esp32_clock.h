#pragma once

#include "can/i_clock.h"

namespace esp_obd::platform::esp32 {

class Esp32Clock : public can::IClock {
 public:
  can::Milliseconds now() const override;
};

}  // namespace esp_obd::platform::esp32
