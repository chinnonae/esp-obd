#include "can/can_frame.h"

namespace esp_obd::can {

namespace {

std::optional<CanFrame> makeDataFrame(uint32_t id, bool extended,
                                       const std::array<uint8_t, 8>& data, uint8_t dlc) {
  if (!isValidDlc(dlc)) {
    return std::nullopt;
  }
  if (extended ? !isValidExtendedId(id) : !isValidStandardId(id)) {
    return std::nullopt;
  }

  CanFrame frame;
  frame.id = id;
  frame.extended = extended;
  frame.remoteRequest = false;
  frame.dlc = dlc;
  frame.data = data;
  return frame;
}

std::optional<CanFrame> makeRemoteFrame(uint32_t id, bool extended, uint8_t dlc) {
  if (!isValidDlc(dlc)) {
    return std::nullopt;
  }
  if (extended ? !isValidExtendedId(id) : !isValidStandardId(id)) {
    return std::nullopt;
  }

  CanFrame frame;
  frame.id = id;
  frame.extended = extended;
  frame.remoteRequest = true;
  frame.dlc = dlc;
  return frame;
}

}  // namespace

std::optional<CanFrame> makeStandardFrame(uint32_t id, const std::array<uint8_t, 8>& data,
                                           uint8_t dlc) {
  return makeDataFrame(id, /*extended=*/false, data, dlc);
}

std::optional<CanFrame> makeExtendedFrame(uint32_t id, const std::array<uint8_t, 8>& data,
                                           uint8_t dlc) {
  return makeDataFrame(id, /*extended=*/true, data, dlc);
}

std::optional<CanFrame> makeStandardRemoteFrame(uint32_t id, uint8_t dlc) {
  return makeRemoteFrame(id, /*extended=*/false, dlc);
}

std::optional<CanFrame> makeExtendedRemoteFrame(uint32_t id, uint8_t dlc) {
  return makeRemoteFrame(id, /*extended=*/true, dlc);
}

}  // namespace esp_obd::can
