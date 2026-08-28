#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "can/can_config.h"
#include "can/can_filter.h"
#include "can/can_frame.h"
#include "can/i_can_port.h"
#include "can/obd_addresses.h"
#include "isotp/isotp_pci.h"
#include "isotp/isotp_receive.h"
#include "isotp/isotp_transmit.h"

// Combines CAN addressing and the ISO-TP TX/RX state machines into one
// request -> responders transaction. Depends only on can/ and isotp/; must
// not depend on elm/ -- see docs/ARCHITECTURE.md. Advances only through
// start()/startAutoSearch()/poll(now), never delay(): a request spans
// multiple poll() ticks exactly like the layers underneath it.

namespace esp_obd::diagnostic {

// "Collect responses from up to eight ECUs" (ELM_COMMAND_BEHAVIOR.md
// section 2.5).
inline constexpr size_t kMaxResponders = 8;

struct Responder {
  uint32_t sourceId = 0;
  bool extended = false;
  std::array<uint8_t, isotp::kMaxPayloadBytes> payload{};
  size_t payloadLength = 0;
  std::array<can::CanFrame, isotp::kMaxRawFrames> rawFrames{};
  size_t rawFrameCount = 0;
};

enum class DiagnosticOutcome {
  Complete,
  NoData,
  BusError,
  UnableToConnect,
};

struct DiagnosticResult {
  DiagnosticOutcome outcome = DiagnosticOutcome::NoData;
  std::array<Responder, kMaxResponders> responders{};
  size_t responderCount = 0;
  // Set only when startAutoSearch() found a working protocol -- the caller
  // (T09, once it exists) is responsible for persisting this into
  // ElmSession; this layer must not depend on elm/.
  std::optional<can::obd::ObdCanProtocol> connectedProtocol;
};

// Populated by the caller from ElmSession + the parsed hex request text.
// Plain fields rather than an ElmSession reference, since this layer must
// not depend on elm/.
struct DiagnosticRequest {
  uint32_t requestId = 0;
  bool requestIdIsExtendedCan = false;
  const uint8_t* payload = nullptr;  // caller-owned; must outlive the transaction
  size_t payloadLength = 0;
  std::optional<uint8_t> maxResponses;  // an odd-length hex request's trailing nibble

  std::optional<uint32_t> explicitReceiveAddress;  // ATCRA
  std::optional<can::CanFilter> receiveFilter;      // ATCF/ATCM

  bool extendedAddressingEnabled = false;  // ATCEA
  uint8_t transmitExtendedAddressByte = 0;  // ATCEAhh
  uint8_t requiredExtendedAddressByte = 0;  // ATCERhh

  bool sendAutomaticFlowControl = true;  // ATCFC0/ATCFC1
  uint8_t flowControlBlockSize = 0;
  uint8_t flowControlStMin = 0;

  can::Milliseconds responseTimeoutMs = 200;
};

class DiagnosticTransport {
 public:
  explicit DiagnosticTransport(can::ICanPort& port) : port_(port) {}

  // Executes one request against a fixed, already-connected protocol.
  // request.requestId/requestIdIsExtendedCan select functional vs.
  // physical: a request id equal to that protocol's functional broadcast
  // id is functional (collects up to kMaxResponders, or
  // request.maxResponses if set); anything else is physical (stops after
  // its first complete response, or request.maxResponses if set).
  void start(can::Milliseconds now, const DiagnosticRequest& request,
             can::obd::ObdCanProtocol protocol);

  // Auto-search (SP0, not yet connected): tries the protocols in
  // can::obd::kAutoSearchOrder in turn, reconfiguring the CAN port for
  // each, until one yields a complete response. Always functional
  // (overrides request.requestId to each candidate's functional id).
  void startAutoSearch(can::Milliseconds now, const DiagnosticRequest& request);

  // Advances transmission, response collection, and (if active) auto-
  // search. Never blocks; call every loop tick until finished().
  void poll(can::Milliseconds now);

  bool finished() const { return finished_; }
  const DiagnosticResult& result() const { return result_; }

 private:
  void beginAttempt(can::Milliseconds now, can::obd::ObdCanProtocol protocol);
  void advanceTransmitPhase(can::Milliseconds now);
  void routeFrame(can::Milliseconds now, const can::CanFrame& frame);
  bool acceptFrame(uint32_t id) const;
  void finalizeCollection(can::Milliseconds now, size_t completeCount);
  void advanceAutoSearch(can::Milliseconds now);
  void finishWithOutcome(DiagnosticOutcome outcome);

  can::ICanPort& port_;
  DiagnosticRequest request_;

  bool autoSearchActive_ = false;
  size_t autoSearchIndex_ = 0;
  can::obd::ObdCanProtocol currentProtocol_ = can::obd::ObdCanProtocol::Iso15765_11bit_500k;
  size_t effectiveMaxResponses_ = 1;

  std::optional<isotp::IsoTpTransmitter> transmitter_;
  bool transmitting_ = true;
  can::Milliseconds overallDeadline_ = 0;

  std::array<std::optional<isotp::IsoTpReceiver>, kMaxResponders> receivers_;
  std::array<uint32_t, kMaxResponders> receiverSourceIds_{};
  size_t activeReceiverCount_ = 0;

  bool finished_ = false;
  DiagnosticResult result_;
};

}  // namespace esp_obd::diagnostic
