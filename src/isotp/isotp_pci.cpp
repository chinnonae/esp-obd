#include "isotp/isotp_pci.h"

namespace esp_obd::isotp {

std::optional<ParsedPci> parsePci(const uint8_t* data, size_t len) {
  if (len == 0) {
    return std::nullopt;
  }

  uint8_t b0 = data[0];
  uint8_t typeNibble = (b0 >> 4) & 0x0F;
  ParsedPci result;

  switch (typeNibble) {
    case 0x0:
      result.type = PciType::SingleFrame;
      result.length = b0 & 0x0F;
      return result;

    case 0x1:
      if (len < 2) {
        return std::nullopt;
      }
      result.type = PciType::FirstFrame;
      result.length = static_cast<uint16_t>(((b0 & 0x0F) << 8) | data[1]);
      return result;

    case 0x2:
      result.type = PciType::ConsecutiveFrame;
      result.sequence = b0 & 0x0F;
      return result;

    case 0x3: {
      if (len < 3) {
        return std::nullopt;
      }
      uint8_t statusNibble = b0 & 0x0F;
      if (statusNibble > 2) {
        return std::nullopt;
      }
      result.type = PciType::FlowControl;
      result.flowStatus = static_cast<FlowStatus>(statusNibble);
      result.blockSize = data[1];
      result.stMin = data[2];
      return result;
    }

    default:
      return std::nullopt;
  }
}

size_t buildFlowControlPci(uint8_t* out, FlowStatus status, uint8_t blockSize, uint8_t stMin) {
  out[0] = static_cast<uint8_t>(0x30 | (static_cast<uint8_t>(status) & 0x0F));
  out[1] = blockSize;
  out[2] = stMin;
  return 3;
}

}  // namespace esp_obd::isotp
