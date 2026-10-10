#pragma once

#include "app/elm_application.h"
#include "app/line_reader.h"
#include "core/fixed_string.h"

// Portable Bluetooth ELM conversation logic: turns received bytes into
// ElmApplication calls and produces the exact bytes to write back (echo,
// reply text, prompt), independent of the actual Bluetooth stack -- see
// docs/ARCHITECTURE.md's BluetoothElmTransport. The ESP32 adapter
// (platform/esp32/) only shuttles real BluetoothSerial bytes through this
// class and writes whatever it returns.

namespace esp_obd::app {

inline constexpr size_t kTransportOutputCapacity = elm::kElmReplyTextCapacity + 32;
using TransportOutput = FixedString<kTransportOutputCapacity>;

// Bluetooth's line idle timeout. Distinct from a diagnostic response
// timeout (ATST): this is "how long we wait mid-line for the next
// keystroke/byte", not "how long we wait for an ECU".
inline constexpr can::Milliseconds kBluetoothLineIdleTimeoutMs = 5000;

class ElmBluetoothSession {
 public:
  explicit ElmBluetoothSession(ElmApplication& app)
      : app_(app), lineReader_(kBluetoothLineIdleTimeoutMs) {}

  // Feeds one received byte. Returns whatever should be written back to
  // the Bluetooth client (may be empty).
  //
  // A byte that arrives while a diagnostic request is still running
  // interrupts it, as on an ELM327: the request is abandoned, "STOPPED" is
  // returned, and the byte is discarded (like the byte that stops monitor
  // mode). If the reply had in fact already arrived, it is returned first
  // and the byte is handled normally.
  //
  // After ATZ/ATWS, bytes are discarded until the next poll(): the caller
  // feeds every byte it already has before polling, so this drops exactly
  // the input that was received before the reset reply went out (e.g. a
  // client sending "ATZ\rATE0\r" in one write gets only the ATZ reply).
  TransportOutput onByte(can::Milliseconds now, uint8_t byte);

  // Advances line-idle timeout tracking and any in-flight diagnostic
  // transaction. Returns whatever should be written back (may be empty).
  TransportOutput poll(can::Milliseconds now);

  // Resets only transport-level state (the line buffer): per
  // docs/tasks/08-transports-and-debug.md, a disconnect/reconnect does not
  // reset ElmSession -- command settings survive it, same as real
  // hardware where they aren't tied to the Bluetooth link layer.
  void onDisconnected() { lineReader_.notifyDisconnected(); }

 private:
  TransportOutput handleLineEvent(can::Milliseconds now, const LineEvent& event);
  TransportOutput finishDiagnosticIfReady();

  ElmApplication& app_;
  LineReader lineReader_;
  bool discardUntilPoll_ = false;  // set by ATZ/ATWS
};

}  // namespace esp_obd::app
