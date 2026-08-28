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

  // Executes one complete input line. For most AT commands, returns the
  // final reply immediately (diagnosticPending() stays false). For a
  // valid hex request, starts a diagnostic transaction and returns a
  // reply with kind == DiagnosticRequest, appendPrompt == false -- check
  // diagnosticPending() right after: it may already be false (a Single
  // Frame request/response usually completes within this same call), in
  // which case takeDiagnosticReply() has the real reply now. Otherwise
  // call poll(now) on subsequent ticks until it returns true. ATCS/ATRTR
  // (which need live ICanPort access the elm/ layer doesn't have) are
  // likewise resolved to a final reply within this same call.
  elm::ElmReply execute(can::Milliseconds now, const char* rawLine);

  // Advances any in-flight diagnostic transaction. Returns true once a
  // reply is ready via takeDiagnosticReply(). Never blocks; a no-op when
  // nothing is pending.
  bool poll(can::Milliseconds now);

  bool diagnosticPending() const { return diagnosticPending_; }
  elm::ElmReply takeDiagnosticReply() const { return readyReply_; }

  bool monitorActive() const { return engine_.session().monitorActive; }

  // Reads one frame from the CAN port (if any) and, if it matches the
  // current monitor filter, returns it formatted as one line (already
  // including the response ending, no prompt). Empty otherwise or when
  // not in monitor mode. Never blocks.
  elm::ElmReplyText pollMonitor(can::Milliseconds now);

  // Leaves monitor mode (any received byte does this -- see
  // app/ElmBluetoothSession) and restores the CAN port's normal mode if
  // ATCSM1 had put it into listen-only.
  void stopMonitor();

  can::ICanPort& canPort() { return canPort_; }
  elm::ElmCommandEngine& engine() { return engine_; }

 private:
  void startDiagnostic(can::Milliseconds now, const elm::ElmReply& requestReply);
  elm::ElmReply formatDiagnosticResult(const diagnostic::DiagnosticResult& result);
  elm::ElmReply resolveCanStatusRequest();
  elm::ElmReply resolveSendRtrRequest();
  void syncMonitorCanMode();

  can::ICanPort& canPort_;
  elm::ElmCommandEngine engine_;
  diagnostic::DiagnosticTransport diagnosticTransport_;

  bool diagnosticPending_ = false;
  elm::ElmReply readyReply_;
  std::array<uint8_t, isotp::kMaxPayloadBytes> pendingPayload_{};
};

}  // namespace esp_obd::app
