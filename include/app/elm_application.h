#pragma once

#include "can/i_can_port.h"
#include "core/i_settings_store.h"
#include "elm/elm_command_engine.h"

// Composition root: the only component that knows about both a transport
// and the command engine (see docs/ARCHITECTURE.md). Portable -- must not
// depend on ESP32 headers; main.cpp constructs the concrete adapters and
// this object, then (once T08 adds a transport) calls poll() from loop().
//
// T07 only builds the shell: it owns the adapters and the command engine
// and can execute a line end to end for AT commands. Turning a
// DiagnosticRequest-kind ElmReply into an actual DiagnosticTransport
// transaction -- and the resulting async reply lifecycle -- is T08's
// integration job, once a real transport exists to receive it.

namespace esp_obd::app {

class ElmApplication {
 public:
  ElmApplication(can::ICanPort& canPort, core::ISettingsStore& settingsStore)
      : canPort_(canPort), engine_(settingsStore) {}

  elm::ElmReply execute(const char* rawLine) { return engine_.execute(rawLine); }

  // No-op today; T08 extends this to advance any in-flight diagnostic
  // transaction and monitor-mode lifecycle once a transport exists.
  void poll(can::Milliseconds /*now*/) {}

  can::ICanPort& canPort() { return canPort_; }
  elm::ElmCommandEngine& engine() { return engine_; }

 private:
  can::ICanPort& canPort_;
  elm::ElmCommandEngine engine_;
};

}  // namespace esp_obd::app
