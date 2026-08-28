#include "elm/protocol_mapping.h"

namespace esp_obd::elm {

std::optional<can::obd::ObdCanProtocol> toObdCanProtocol(ElmProtocol protocol) {
  switch (protocol) {
    case ElmProtocol::Iso15765_11bit_500k:
      return can::obd::ObdCanProtocol::Iso15765_11bit_500k;
    case ElmProtocol::Iso15765_29bit_500k:
      return can::obd::ObdCanProtocol::Iso15765_29bit_500k;
    case ElmProtocol::Iso15765_11bit_250k:
      return can::obd::ObdCanProtocol::Iso15765_11bit_250k;
    case ElmProtocol::Iso15765_29bit_250k:
      return can::obd::ObdCanProtocol::Iso15765_29bit_250k;
    default:
      return std::nullopt;
  }
}

ElmProtocol fromObdCanProtocol(can::obd::ObdCanProtocol protocol) {
  switch (protocol) {
    case can::obd::ObdCanProtocol::Iso15765_11bit_500k:
      return ElmProtocol::Iso15765_11bit_500k;
    case can::obd::ObdCanProtocol::Iso15765_29bit_500k:
      return ElmProtocol::Iso15765_29bit_500k;
    case can::obd::ObdCanProtocol::Iso15765_11bit_250k:
      return ElmProtocol::Iso15765_11bit_250k;
    case can::obd::ObdCanProtocol::Iso15765_29bit_250k:
    default:
      return ElmProtocol::Iso15765_29bit_250k;
  }
}

}  // namespace esp_obd::elm
