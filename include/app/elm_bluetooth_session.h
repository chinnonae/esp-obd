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
};

}  // namespace esp_obd::app
