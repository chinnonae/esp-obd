#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>

// Portable, hardware-independent CAN frame value type and its validated
// factory helpers. No Arduino/TWAI type may appear here or in anything this
// header depends on -- see docs/ARCHITECTURE.md's dependency rules.

namespace esp_obd::can {

inline constexpr uint32_t kStandardIdMax = 0x7FF;
inline constexpr uint32_t kExtendedIdMax = 0x1FFFFFFF;
inline constexpr uint8_t kMaxDlc = 8;

constexpr bool isValidStandardId(uint32_t id) { return id <= kStandardIdMax; }
constexpr bool isValidExtendedId(uint32_t id) { return id <= kExtendedIdMax; }
constexpr bool isValidDlc(uint8_t dlc) { return dlc <= kMaxDlc; }

struct CanFrame {
  uint32_t id = 0;
  bool extended = false;
  bool remoteRequest = false;
  uint8_t dlc = 0;
  std::array<uint8_t, 8> data{};
};

// Canonical factories: reject an out-of-range id or dlc by returning
// std::nullopt, so an invalid CanFrame can never be constructed and passed
// to ICanPort::send(). Only the first `dlc` bytes of `data` are meaningful;
// trailing bytes should be zero.
std::optional<CanFrame> makeStandardFrame(uint32_t id, const std::array<uint8_t, 8>& data,
                                           uint8_t dlc);
std::optional<CanFrame> makeExtendedFrame(uint32_t id, const std::array<uint8_t, 8>& data,
                                           uint8_t dlc);

// Remote-request frames carry no payload; only `dlc` (the requested length)
// is meaningful.
std::optional<CanFrame> makeStandardRemoteFrame(uint32_t id, uint8_t dlc);
std::optional<CanFrame> makeExtendedRemoteFrame(uint32_t id, uint8_t dlc);

// Convenience overloads for a literal payload, e.g.
// makeStandardFrame(0x7DF, {0x02, 0x01, 0x0C}); dlc is bytes.size().
inline std::optional<CanFrame> makeStandardFrame(uint32_t id,
                                                  std::initializer_list<uint8_t> bytes) {
  if (bytes.size() > kMaxDlc) {
    return std::nullopt;
  }
  std::array<uint8_t, 8> data{};
  size_t i = 0;
  for (uint8_t b : bytes) {
    data[i++] = b;
  }
  return makeStandardFrame(id, data, static_cast<uint8_t>(bytes.size()));
}

inline std::optional<CanFrame> makeExtendedFrame(uint32_t id,
                                                  std::initializer_list<uint8_t> bytes) {
  if (bytes.size() > kMaxDlc) {
    return std::nullopt;
  }
  std::array<uint8_t, 8> data{};
  size_t i = 0;
  for (uint8_t b : bytes) {
    data[i++] = b;
  }
  return makeExtendedFrame(id, data, static_cast<uint8_t>(bytes.size()));
}

}  // namespace esp_obd::can
