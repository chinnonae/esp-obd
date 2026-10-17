#pragma once

#include <cstddef>
#include <cstdint>

#include "can/i_clock.h"
#include "core/fixed_string.h"

// Reusable bounded line assembly, shared by the Bluetooth ELM transport
// and the UART debug console (see docs/ARCHITECTURE.md). LineReader
// merely returns events; it does not know what a line means. Advances
// only through onByte()/poll(now) -- never delay().

namespace esp_obd::app {

inline constexpr size_t kLineReaderCapacity = 64;
using LineReaderText = FixedString<kLineReaderCapacity>;

enum class LineEventKind {
  None,        // byte accumulated; no complete line yet
  Line,        // a complete line is ready (CR seen); a following LF is swallowed
  Overflow,    // too many bytes without a CR; buffer discarded
  TimedOut,    // too long since the last byte, with a partial line pending
  Disconnected,  // the owning transport detected a disconnect; buffer discarded
};

struct LineEvent {
  LineEventKind kind = LineEventKind::None;
  LineReaderText text;  // meaningful only when kind == Line
};

class LineReader {
 public:
  explicit LineReader(can::Milliseconds idleTimeoutMs) : idleTimeoutMs_(idleTimeoutMs) {}

  // Feeds one received byte. A linefeed immediately following a CR is
  // ignored (docs/ELM_COMMAND_BEHAVIOR.md section 1.1).
  LineEvent onByte(can::Milliseconds now, uint8_t byte);

  // Detects "too long since the last byte with a partial line pending".
  // Never fires with an empty buffer -- nothing pending to time out.
  LineEvent poll(can::Milliseconds now);

  // Called by the owning transport when it detects a disconnect.
  LineEvent notifyDisconnected();

 private:
  can::Milliseconds idleTimeoutMs_;
  LineReaderText buffer_;
  bool justSawCr_ = false;
  bool hasPending_ = false;
  can::Milliseconds lastByteTime_ = 0;
};

}  // namespace esp_obd::app
