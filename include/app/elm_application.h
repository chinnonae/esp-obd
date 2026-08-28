#pragma once

#include <array>

#include "can/i_can_port.h"
#include "core/i_settings_store.h"
#include "diagnostic/diagnostic_transport.h"
#include "elm/elm_command_engine.h"
#include "isotp/isotp_pci.h"

// Composition root: the only component that knows about both a transport
// and the command engine (see docs/ARCHITECTURE.md). Portable -- must not
// depend on ESP32 headers; main.cpp constructs the concrete adapters and
// this object, then calls poll() from loop().

namespace esp_obd::app {

class ElmApplication {
 public:
  ElmApplication(can::ICanPort& canPort, core::ISettingsStore& settingsStore)
      : canPort_(canPort), engine_(settingsStore), diagnosticTransport_(canPort) {}

  // Executes one complete input line. For an AT command, returns the
  // final reply immediately (diagnosticPending() stays false). For a
  // valid hex request, starts a diagnostic transaction and returns a
  // reply with kind == DiagnosticRequest, appendPrompt == false -- check
  // diagnosticPending() right after: it may already be false (a Single
  // Frame request/response usually completes within this same call), in
  // which case takeDiagnosticReply() has the real reply now. Otherwise
  // call poll(now) on subsequent ticks until it returns true.
  elm::ElmReply execute(can::Milliseconds now, const char* rawLine);

  // Advances any in-flight diagnostic transaction. Returns true once a
  // reply is ready via takeDiagnosticReply(). Never blocks; a no-op when
  // nothing is pending.
  bool poll(can::Milliseconds now);

  bool diagnosticPending() const { return diagnosticPending_; }
  elm::ElmReply takeDiagnosticReply() const { return readyReply_; }

  can::ICanPort& canPort() { return canPort_; }
  elm::ElmCommandEngine& engine() { return engine_; }

 private:
  void startDiagnostic(can::Milliseconds now, const elm::ElmReply& requestReply);
  elm::ElmReply formatDiagnosticResult(const diagnostic::DiagnosticResult& result);

  can::ICanPort& canPort_;
  elm::ElmCommandEngine engine_;
  diagnostic::DiagnosticTransport diagnosticTransport_;

  bool diagnosticPending_ = false;
  elm::ElmReply readyReply_;
  std::array<uint8_t, isotp::kMaxPayloadBytes> pendingPayload_{};
};

}  // namespace esp_obd::app
