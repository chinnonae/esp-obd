#pragma once

#include <cstdint>
#include <optional>

#include "can/can_filter.h"
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

// ATSP/ATTP (T09). Only the in-scope CAN protocols exist; ATSP1-5/A-C are
// permanently unsupported per docs/ELM_COMMAND_BEHAVIOR.md section 2.2.
enum class ElmProtocol {
  AutomaticSearch,      // SP0 (default)
  Iso15765_11bit_500k,  // SP6
  Iso15765_29bit_500k,  // SP7
  Iso15765_11bit_250k,  // SP8
  Iso15765_29bit_250k,  // SP9
};

struct ElmSession {
  // --- T03: core session commands ---
  bool echoEnabled = true;
  bool linefeedsEnabled = false;
  bool spacesEnabled = true;
  bool headersEnabled = false;
  bool displayDlcEnabled = false;
  bool responsesEnabled = true;

  // --- T09: protocol, addressing, formatting, monitoring ---
  bool automaticFormattingEnabled = true;   // CAF1
  bool automaticFlowControlEnabled = true;  // CFC1
  bool allowLongMessagesEnabled = false;    // NL off
  AdaptiveTiming adaptiveTiming = AdaptiveTiming::Mode1;
  ElmProtocol protocol = ElmProtocol::AutomaticSearch;
  bool protocolConnected = false;
  uint32_t requestId = 0x7DF;
  std::optional<uint32_t> receiveAddress;  // ATCRA; cleared by default
  std::optional<can::CanFilter> idFilter;  // ATCF/ATCM; cleared by default
  can::Milliseconds responseTimeoutMs = 200;  // 0x32 * 4
  std::optional<uint32_t> customHeaderId;     // ATSH; unset by default
  bool extendedAddressingEnabled = false;     // ATCEA
  uint8_t extendedAddressByte = 0;
  bool monitorActive = false;

  void resetToDefaults() { *this = ElmSession{}; }
};

inline bool operator==(const ElmSession& a, const ElmSession& b) {
  return a.echoEnabled == b.echoEnabled && a.linefeedsEnabled == b.linefeedsEnabled &&
         a.spacesEnabled == b.spacesEnabled && a.headersEnabled == b.headersEnabled &&
         a.displayDlcEnabled == b.displayDlcEnabled &&
         a.responsesEnabled == b.responsesEnabled &&
         a.automaticFormattingEnabled == b.automaticFormattingEnabled &&
         a.automaticFlowControlEnabled == b.automaticFlowControlEnabled &&
         a.allowLongMessagesEnabled == b.allowLongMessagesEnabled &&
         a.adaptiveTiming == b.adaptiveTiming && a.protocol == b.protocol &&
         a.protocolConnected == b.protocolConnected && a.requestId == b.requestId &&
         a.receiveAddress == b.receiveAddress && a.idFilter == b.idFilter &&
         a.responseTimeoutMs == b.responseTimeoutMs &&
         a.customHeaderId == b.customHeaderId &&
         a.extendedAddressingEnabled == b.extendedAddressingEnabled &&
         a.extendedAddressByte == b.extendedAddressByte &&
         a.monitorActive == b.monitorActive;
}

}  // namespace esp_obd::elm
