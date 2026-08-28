#include "elm/at_commands_addressing.h"

#include <cstring>

#include "can/can_frame.h"
#include "elm/elm_errors.h"
#include "elm/elm_formatter.h"
#include "elm/elm_parser.h"

namespace esp_obd::elm {

namespace {

bool eq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

// A CAN id argument is either exactly 3 hex digits (11-bit, <= 0x7FF) or
// exactly 8 hex digits (29-bit, <= 0x1FFFFFFF). Shared by ATSH and ATFCSH.
std::optional<uint32_t> parseCanIdArgument(const char* text) {
  size_t len = std::strlen(text);
  if (len == 3) {
    auto value = parseFixedWidthHex(text, 3);
    if (!value.has_value() || !can::isValidStandardId(*value)) return std::nullopt;
    return value;
  }
  if (len == 8) {
    auto value = parseFixedWidthHex(text, 8);
    if (!value.has_value() || !can::isValidExtendedId(*value)) return std::nullopt;
    return value;
  }
  return std::nullopt;
}

}  // namespace

std::optional<ElmReply> dispatchAddressingCommand(ElmSession& session, const char* r) {
  if (std::strncmp(r, "SH", 2) == 0) {
    auto id = parseCanIdArgument(r + 2);
    if (!id.has_value()) return std::nullopt;
    session.customHeaderId = *id;
    return textReply(session, kOkText);
  }

  if (std::strncmp(r, "CP", 2) == 0) {
    auto value = parseFixedWidthHex(r + 2, 2);
    if (!value.has_value()) return std::nullopt;
    session.priorityBits = static_cast<uint8_t>(*value & 0x1F);
    return textReply(session, kOkText);
  }

  if (eq(r, "CRA") || eq(r, "AR")) {
    session.receiveAddress = std::nullopt;
    session.idFilter = std::nullopt;
    return textReply(session, kOkText);
  }
  if (std::strncmp(r, "CRA", 3) == 0) {
    auto id = parseCanIdArgument(r + 3);
    if (!id.has_value()) return std::nullopt;
    session.receiveAddress = *id;
    return textReply(session, kOkText);
  }

  if (std::strncmp(r, "CF", 2) == 0 && r[2] != 'C') {  // "CF..." but not "CFC0/1"
    auto id = parseCanIdArgument(r + 2);
    if (!id.has_value()) return std::nullopt;
    can::CanFilter filter = session.idFilter.value_or(can::CanFilter{});
    filter.filterValue = *id;
    session.idFilter = filter;
    return textReply(session, kOkText);
  }

  if (std::strncmp(r, "CM", 2) == 0) {
    auto id = parseCanIdArgument(r + 2);
    if (!id.has_value()) return std::nullopt;
    can::CanFilter filter = session.idFilter.value_or(can::CanFilter{});
    filter.mask = *id;
    session.idFilter = filter;
    return textReply(session, kOkText);
  }

  if (eq(r, "CAF0")) { session.automaticFormattingEnabled = false; return textReply(session, kOkText); }
  if (eq(r, "CAF1")) { session.automaticFormattingEnabled = true; return textReply(session, kOkText); }
  if (eq(r, "CFC0")) { session.automaticFlowControlEnabled = false; return textReply(session, kOkText); }
  if (eq(r, "CFC1")) { session.automaticFlowControlEnabled = true; return textReply(session, kOkText); }

  if (eq(r, "FCSM0")) { session.flowControlMode = FlowControlMode::Automatic; return textReply(session, kOkText); }
  if (eq(r, "FCSM1")) { session.flowControlMode = FlowControlMode::ManualHeaderAndData; return textReply(session, kOkText); }
  if (eq(r, "FCSM2")) { session.flowControlMode = FlowControlMode::ManualDataAutoHeader; return textReply(session, kOkText); }

  if (std::strncmp(r, "FCSH", 4) == 0) {
    auto id = parseCanIdArgument(r + 4);
    if (!id.has_value()) return std::nullopt;
    session.manualFlowControlId = *id;
    return textReply(session, kOkText);
  }

  if (std::strncmp(r, "FCSD", 4) == 0) {
    const char* digits = r + 4;
    size_t len = std::strlen(digits);
    if (len == 0 || len % 2 != 0 || len > 10) return std::nullopt;  // 1..5 bytes
    uint8_t bytes[5] = {};
    size_t byteCount = decodeHexBytes(digits, bytes, sizeof(bytes));
    if (byteCount * 2 != len) return std::nullopt;  // rejects invalid hex partway through
    session.manualFlowControlData.fill(0);
    for (size_t i = 0; i < byteCount; ++i) session.manualFlowControlData[i] = bytes[i];
    session.manualFlowControlDataLen = static_cast<uint8_t>(byteCount);
    return textReply(session, kOkText);
  }

  if (eq(r, "CEA")) {
    session.extendedAddressingEnabled = false;
    return textReply(session, kOkText);
  }
  if (std::strncmp(r, "CEA", 3) == 0) {
    auto value = parseFixedWidthHex(r + 3, 2);
    if (!value.has_value()) return std::nullopt;
    session.extendedAddressingEnabled = true;
    session.extendedAddressByte = static_cast<uint8_t>(*value);
    return textReply(session, kOkText);
  }

  if (std::strncmp(r, "CER", 3) == 0) {
    auto value = parseFixedWidthHex(r + 3, 2);
    if (!value.has_value()) return std::nullopt;
    session.requiredExtendedAddressByte = static_cast<uint8_t>(*value);
    return textReply(session, kOkText);
  }

  return std::nullopt;
}

}  // namespace esp_obd::elm
