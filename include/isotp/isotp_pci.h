#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

// ISO-TP (ISO 15765-2) protocol-control-information parsing shared by the
// receive (T04) and transmit (T05) state machines. Classical CAN only: no
// FD escape sequences.

namespace esp_obd::isotp {

// Bounded payload limit shared by RX (T04) and TX (T05): comfortably covers
// real OBD-II/UDS responses/requests (VIN, DTC dumps, calibration IDs, UDS
// writes) while staying well under the ISO-TP theoretical max of 4095, so
// "declared length larger than the limit" stays reachable and testable.
inline constexpr size_t kMaxPayloadBytes = 255;

enum class PciType : uint8_t {
  SingleFrame = 0x0,
  FirstFrame = 0x1,
  ConsecutiveFrame = 0x2,
  FlowControl = 0x3,
};

enum class FlowStatus : uint8_t {
  ContinueToSend = 0,
  Wait = 1,
  Overflow = 2,
};

struct ParsedPci {
  PciType type = PciType::SingleFrame;
  uint16_t length = 0;    // SingleFrame: 0-7. FirstFrame: 8-4095.
  uint8_t sequence = 0;   // ConsecutiveFrame: 0-15.
  FlowStatus flowStatus = FlowStatus::ContinueToSend;  // FlowControl only.
  uint8_t blockSize = 0;                               // FlowControl only.
  uint8_t stMin = 0;                                   // FlowControl only.
};

// Parses the PCI at the start of `data` (already past any ISO-TP extended
// addressing byte -- the caller strips that first). Returns std::nullopt
// for a truncated frame or an invalid type/status nibble.
std::optional<ParsedPci> parsePci(const uint8_t* data, size_t len);

// Writes a 3-byte Flow Control PCI ([PCI][BS][STmin]) to `out` (which must
// have room for 3 bytes) and returns 3.
size_t buildFlowControlPci(uint8_t* out, FlowStatus status, uint8_t blockSize, uint8_t stMin);

// Converts an ISO-TP STmin byte to whole milliseconds: 0x00-0x7F is 0-127ms
// directly. 0xF1-0xF9 (100-900 *micro*seconds) round up to 1ms, since this
// project's clock resolution is milliseconds. The reserved ranges
// (0x80-0xF0, 0xFA-0xFF) are treated as the slowest legal value, 127ms, to
// fail safe rather than send too fast.
uint16_t stMinToMilliseconds(uint8_t stMin);

}  // namespace esp_obd::isotp
