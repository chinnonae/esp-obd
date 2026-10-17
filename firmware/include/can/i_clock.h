#pragma once

#include <cstdint>

namespace esp_obd::can {

using Milliseconds = uint32_t;

class IClock {
 public:
  virtual Milliseconds now() const = 0;
  virtual ~IClock() = default;
};

}  // namespace esp_obd::can
