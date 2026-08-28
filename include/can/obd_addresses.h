#pragma once

#include <cstdint>

// Default CAN-only OBD-II addressing (ISO 15765-4). See
// docs/ELM_COMMAND_BEHAVIOR.md sections 1.3 and 2.2 for the request-side
// defaults (e.g. ATSP6-9); this header covers the response side.

namespace esp_obd::can::obd {

// 11-bit addressing (ATSP6/ATSP8): functional request 0x7DF, ECU responses
// occupy the fixed range 0x7E8-0x7EF inclusive.
inline constexpr uint32_t kFunctionalRequestId11Bit = 0x7DF;
inline constexpr uint32_t kResponseRangeStart11Bit = 0x7E8;
inline constexpr uint32_t kResponseRangeEnd11Bit = 0x7EF;

// 29-bit addressing (ATSP7/ATSP9): tester address is fixed at 0xF1.
// Functional request is 0x18DB33F1; physical ECU responses to the tester
// occupy 0x18DAF100-0x18DAF1FF inclusive (target=tester 0xF1, source=ECU).
inline constexpr uint32_t kTesterAddress29Bit = 0xF1;
inline constexpr uint32_t kFunctionalRequestId29Bit = 0x18DB33F1;
inline constexpr uint32_t kResponseRangeStart29Bit = 0x18DAF100;
inline constexpr uint32_t kResponseRangeEnd29Bit = 0x18DAF1FF;

constexpr bool isDefaultObdResponse11Bit(uint32_t id) {
  return id >= kResponseRangeStart11Bit && id <= kResponseRangeEnd11Bit;
}

constexpr bool isDefaultObdResponse29Bit(uint32_t id) {
  return id >= kResponseRangeStart29Bit && id <= kResponseRangeEnd29Bit;
}

}  // namespace esp_obd::can::obd
