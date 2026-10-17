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

void applyProtocol(ElmSession& session, ElmProtocol protocol) {
  session.protocol = protocol;
  session.protocolDiscoveredViaAutoSearch = false;
  auto wire = toObdCanProtocol(protocol);
  session.protocolConnected = wire.has_value();
  session.requestId = wire.has_value() ? can::obd::functionalRequestId(*wire)
                                       : can::obd::kFunctionalRequestId11Bit;
}

void resetToDefaultsKeepingSavedProtocol(ElmSession& session) {
  ElmProtocol saved = session.savedProtocol;
  session.resetToDefaults();
  session.savedProtocol = saved;
  applyProtocol(session, saved);
}

RequestCanId requestCanIdFor(const ElmSession& session, can::obd::ObdCanProtocol protocol) {
  RequestCanId result;
  result.extended = can::obd::isExtendedCan(protocol);
  if (!session.customHeader.has_value()) {
    result.id = can::obd::functionalRequestId(protocol);
  } else if (result.extended) {
    result.id = (static_cast<uint32_t>(session.priorityBits & 0x1F) << 24) | *session.customHeader;
  } else {
    result.id = *session.customHeader & can::kStandardIdMax;
  }
  return result;
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
