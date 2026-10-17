#include "app/line_reader.h"

namespace esp_obd::app {

LineEvent LineReader::onByte(can::Milliseconds now, uint8_t byte) {
  lastByteTime_ = now;

  if (justSawCr_) {
    justSawCr_ = false;
    if (byte == '\n') {
      return LineEvent{};
    }
  }

  if (byte == '\r') {
    LineEvent event;
    event.kind = LineEventKind::Line;
    event.text = buffer_;
    buffer_.clear();
    justSawCr_ = true;
    hasPending_ = false;
    return event;
  }

  if (buffer_.size() >= kLineReaderCapacity) {
    buffer_.clear();
    justSawCr_ = false;
    hasPending_ = false;
    LineEvent event;
    event.kind = LineEventKind::Overflow;
    return event;
  }

  buffer_ += static_cast<char>(byte);
  hasPending_ = true;
  return LineEvent{};
}

LineEvent LineReader::poll(can::Milliseconds now) {
  if (!hasPending_) {
    return LineEvent{};
  }
  if (now - lastByteTime_ < idleTimeoutMs_) {
    return LineEvent{};
  }
  buffer_.clear();
  justSawCr_ = false;
  hasPending_ = false;
  LineEvent event;
  event.kind = LineEventKind::TimedOut;
  return event;
}

LineEvent LineReader::notifyDisconnected() {
  buffer_.clear();
  justSawCr_ = false;
  hasPending_ = false;
  LineEvent event;
  event.kind = LineEventKind::Disconnected;
  return event;
}

}  // namespace esp_obd::app
