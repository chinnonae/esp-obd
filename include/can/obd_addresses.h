#pragma once

#include <cstdint>

#include "can/can_config.h"

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

// The four in-scope CAN-only protocols (ATSP6-9). No "automatic search"
// member here -- that is ELM session state (elm::ElmProtocol), not a wire
// configuration; this enum is usable by diagnostic/ precisely because it
// must not depend on elm/.
enum class ObdCanProtocol {
  Iso15765_11bit_500k,  // SP6
  Iso15765_29bit_500k,  // SP7
  Iso15765_11bit_250k,  // SP8
  Iso15765_29bit_250k,  // SP9
};

// The auto-search order (docs/ELM_COMMAND_BEHAVIOR.md section 2.2): "next
// request tries 6, 7, 8, 9 in that order."
inline constexpr ObdCanProtocol kAutoSearchOrder[4] = {
    ObdCanProtocol::Iso15765_11bit_500k,
    ObdCanProtocol::Iso15765_29bit_500k,
    ObdCanProtocol::Iso15765_11bit_250k,
    ObdCanProtocol::Iso15765_29bit_250k,
};

constexpr bool isExtendedCan(ObdCanProtocol protocol) {
  return protocol == ObdCanProtocol::Iso15765_29bit_500k ||
         protocol == ObdCanProtocol::Iso15765_29bit_250k;
}

constexpr can::Bitrate bitrateFor(ObdCanProtocol protocol) {
  return (protocol == ObdCanProtocol::Iso15765_11bit_500k ||
          protocol == ObdCanProtocol::Iso15765_29bit_500k)
             ? can::Bitrate::Bitrate500k
             : can::Bitrate::Bitrate250k;
}

constexpr uint32_t functionalRequestId(ObdCanProtocol protocol) {
  return isExtendedCan(protocol) ? kFunctionalRequestId29Bit : kFunctionalRequestId11Bit;
}

constexpr bool isDefaultObdResponse(uint32_t id, ObdCanProtocol protocol) {
  return isExtendedCan(protocol) ? isDefaultObdResponse29Bit(id) : isDefaultObdResponse11Bit(id);
}

// The CAN id a tester sends Flow Control to for a given responder's source
// id: the reverse of the standard ISO 15765-4 request/response pairing.
// 11-bit: response id - 8 (0x7E8 -> 0x7E0, ... 0x7EF -> 0x7E7). 29-bit:
// swap target/source (response 0x18DAF1xx, from ECU xx to tester F1) back
// to request direction (0x18DAxxF1, from tester F1 to ECU xx).
constexpr uint32_t computeFlowControlId(uint32_t responseId, bool extended) {
  if (!extended) {
    return responseId - 8;
  }
  uint32_t ecuAddress = responseId & 0xFF;
  return 0x18DA0000u | (ecuAddress << 8) | kTesterAddress29Bit;
}

}  // namespace esp_obd::can::obd
