#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "can/can_filter.h"
#include "can/can_frame.h"
#include "can/i_clock.h"

// Session-only state. No parser or CAN I/O -- see docs/ARCHITECTURE.md.
//
// Models every row of the defaults table in
// docs/ELM_COMMAND_BEHAVIOR.md section 1.3, so ATZ/ATD/ATWS (T03) can
// reset the whole session at once and later tasks (T09, T11) only need to
// add the handlers that mutate their own fields -- not touch this struct.
// Identity is not modeled here: it is the constant
// esp_obd::build::kElmVersion, not per-session state.

namespace esp_obd::elm {

enum class AdaptiveTiming {
  Off,
  Mode1,
  Mode2,
};

// ATSP/ATTP. Only the in-scope CAN protocols exist; ATSP1-5/A-C are
// permanently unsupported per docs/ELM_COMMAND_BEHAVIOR.md section 2.2.
enum class ElmProtocol {
  AutomaticSearch,      // SP0 (default)
  Iso15765_11bit_500k,  // SP6
  Iso15765_29bit_500k,  // SP7
  Iso15765_11bit_250k,  // SP8
  Iso15765_29bit_250k,  // SP9
};

// ATFCSM0/1/2.
enum class FlowControlMode {
  Automatic,            // FCSM0 (default)
  ManualHeaderAndData,  // FCSM1
  ManualDataAutoHeader,  // FCSM2
};

// ATMA/ATMR/ATMT.
enum class MonitorMode {
  None,
  All,                // ATMA
  ReceivedAddress,     // ATMRhh
  TransmittedAddress,  // ATMThh
};

struct ElmSession {
  // --- T03: core session commands ---
  bool echoEnabled = true;
  bool linefeedsEnabled = false;
  bool spacesEnabled = true;
  bool headersEnabled = false;
  bool displayDlcEnabled = false;
  bool responsesEnabled = true;

  // --- T09: protocol and timeout (ATSP/ATTP/ATDP/ATDPN/ATST/ATAT/ATCTM/ATPC) ---
  ElmProtocol protocol = ElmProtocol::AutomaticSearch;
  bool protocolConnected = false;
  // ATDP/ATDPN's "AUTO, "/"A6".."A9" prefix applies only when the current
  // protocol was reached via auto-search, not a manual ATSP/ATTP.
  bool protocolDiscoveredViaAutoSearch = false;
  uint32_t requestId = 0x7DF;
  can::Milliseconds responseTimeoutMs = 200;  // 0x32 * 4
  AdaptiveTiming adaptiveTiming = AdaptiveTiming::Mode1;
  uint8_t canTimeoutMultiplier = 1;  // ATCTM1/ATCTM5

  // --- T09: addressing and filtering (ATSH/ATCP/ATCRA/ATAR/ATCF/ATCM) ---
  std::optional<uint32_t> customHeaderId;   // ATSH; unset by default
  uint8_t priorityBits = 0x18;              // ATCPhh: 29-bit header priority
  std::optional<uint32_t> receiveAddress;   // ATCRA/ATAR; cleared by default
  std::optional<can::CanFilter> idFilter;   // ATCF/ATCM; cleared by default

  // --- T09: ISO-TP formatting and flow control (ATCAF/ATCFC/ATFCSM/ATFCSH/ATFCSD) ---
  bool automaticFormattingEnabled = true;   // CAF1
  bool automaticFlowControlEnabled = true;  // CFC1
  bool allowLongMessagesEnabled = false;    // NL off
  FlowControlMode flowControlMode = FlowControlMode::Automatic;
  std::optional<uint32_t> manualFlowControlId;  // ATFCSHhhh/hhhhhhhh
  std::array<uint8_t, 5> manualFlowControlData{};
  uint8_t manualFlowControlDataLen = 0;  // ATFCSD

  // --- T09: extended addressing (ATCEA/ATCEAhh/ATCERhh) ---
  bool extendedAddressingEnabled = false;
  uint8_t extendedAddressByte = 0;                     // ATCEAhh: our transmit address
  std::optional<uint8_t> requiredExtendedAddressByte;  // ATCERhh: required in received frames

  // --- T09: monitoring and CAN diagnostics (ATMA/ATMR/ATMT/ATCSM/ATV/ATBD) ---
  bool monitorActive = false;
  MonitorMode monitorMode = MonitorMode::None;
  uint8_t monitorAddressByte = 0;  // for ATMRhh/ATMThh
  bool silentMonitoringEnabled = false;  // ATCSM0/1
  bool variableDlcEnabled = false;       // ATV0/1; false = V0 = fixed 8-byte frames,
                                          // matching T04/T05's current hardcoded padding
  std::optional<can::CanFrame> lastAcceptedReceivedFrame;  // ATBD

  void resetToDefaults() { *this = ElmSession{}; }
};

inline bool operator==(const ElmSession& a, const ElmSession& b) {
  return a.echoEnabled == b.echoEnabled && a.linefeedsEnabled == b.linefeedsEnabled &&
         a.spacesEnabled == b.spacesEnabled && a.headersEnabled == b.headersEnabled &&
         a.displayDlcEnabled == b.displayDlcEnabled &&
         a.responsesEnabled == b.responsesEnabled && a.protocol == b.protocol &&
         a.protocolConnected == b.protocolConnected &&
         a.protocolDiscoveredViaAutoSearch == b.protocolDiscoveredViaAutoSearch &&
         a.requestId == b.requestId &&
         a.responseTimeoutMs == b.responseTimeoutMs && a.adaptiveTiming == b.adaptiveTiming &&
         a.canTimeoutMultiplier == b.canTimeoutMultiplier &&
         a.customHeaderId == b.customHeaderId && a.priorityBits == b.priorityBits &&
         a.receiveAddress == b.receiveAddress && a.idFilter == b.idFilter &&
         a.automaticFormattingEnabled == b.automaticFormattingEnabled &&
         a.automaticFlowControlEnabled == b.automaticFlowControlEnabled &&
         a.allowLongMessagesEnabled == b.allowLongMessagesEnabled &&
         a.flowControlMode == b.flowControlMode &&
         a.manualFlowControlId == b.manualFlowControlId &&
         a.manualFlowControlData == b.manualFlowControlData &&
         a.manualFlowControlDataLen == b.manualFlowControlDataLen &&
         a.extendedAddressingEnabled == b.extendedAddressingEnabled &&
         a.extendedAddressByte == b.extendedAddressByte &&
         a.requiredExtendedAddressByte == b.requiredExtendedAddressByte &&
         a.monitorActive == b.monitorActive && a.monitorMode == b.monitorMode &&
         a.monitorAddressByte == b.monitorAddressByte &&
         a.silentMonitoringEnabled == b.silentMonitoringEnabled &&
         a.variableDlcEnabled == b.variableDlcEnabled;
  // lastAcceptedReceivedFrame is intentionally excluded: it's a
  // diagnostic-transaction side effect, not a "setting" a malformed
  // command could be accused of mutating.
}

}  // namespace esp_obd::elm
