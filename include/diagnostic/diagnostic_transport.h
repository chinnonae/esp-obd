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

// After a "response pending" negative response (7F <sid> 78), how long to
// keep waiting for that ECU's real answer: ISO 14229-2's default P2*server.
inline constexpr can::Milliseconds kResponsePendingTimeoutMs = 5000;

// True for a UDS/OBD negative response with NRC 0x78 (requestCorrectly-
// ReceivedResponsePending): an interim "still working" reply, not an answer.
inline bool isResponsePending(const uint8_t* payload, size_t length) {
  return length == 3 && payload[0] == 0x7F && payload[2] == 0x78;
}

// ATAT0/1/2, mirrored here because this layer must not depend on elm/.
enum class AdaptiveTimingMode {
  Off,         // AT0: wait the full timeout after every response
  Normal,      // AT1: after a response, wait only as long as ECUs usually take
  Aggressive,  // AT2: as Normal, with a tighter margin
};

// Adaptive timing never waits less than this after the latest response.
inline constexpr can::Milliseconds kAdaptiveNormalMinGapMs = 20;
inline constexpr can::Milliseconds kAdaptiveAggressiveMinGapMs = 8;

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
  // The first frame this request transmitted (its Single or First Frame),
  // for ATBD. Unset if nothing was sent.
  std::optional<can::CanFrame> requestFrame;

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

  // ATFCSM1: Flow Control goes to this id instead of the one derived from
  // the responder's id.
  std::optional<uint32_t> manualFlowControlId;
  // ATFCSM1/2: Flow Control carries these bytes; length 0 = generate it.
  std::array<uint8_t, 5> manualFlowControlData{};
  uint8_t manualFlowControlDataLen = 0;

  // ATST x ATCTM. The wait for a first response, and (AT0) the wait
  // restarted after every response.
  can::Milliseconds responseTimeoutMs = 200;
  AdaptiveTimingMode adaptiveTiming = AdaptiveTimingMode::Off;
};

// Forwards everything to the wrapped port, remembering the first frame sent
// since clear() -- used to capture a request's Single/First Frame for ATBD.
class FirstSendRecorder : public can::ICanPort {
 public:
  explicit FirstSendRecorder(can::ICanPort& port) : port_(port) {}
  bool configure(const can::CanConfig& config) override { return port_.configure(config); }
  can::CanResult send(const can::CanFrame& frame, can::Milliseconds timeout) override {
    if (!firstSent_.has_value()) {
      firstSent_ = frame;
    }
    return port_.send(frame, timeout);
  }
  can::ReceiveResult receive() override { return port_.receive(); }
  void discardPending() override { port_.discardPending(); }
  can::CanStatus status() const override { return port_.status(); }

  void clear() { firstSent_.reset(); }
  const std::optional<can::CanFrame>& firstSent() const { return firstSent_; }

 private:
  can::ICanPort& port_;
  std::optional<can::CanFrame> firstSent_;
};

class DiagnosticTransport {
 public:
  explicit DiagnosticTransport(can::ICanPort& port) : port_(port), requestRecorder_(port) {}

  // Executes one request against a fixed, already-connected protocol.
  // request.requestId/requestIdIsExtendedCan select functional vs.
  // physical: a request id equal to that protocol's functional broadcast
  // id is functional (collects up to kMaxResponders, or
  // request.maxResponses if set); anything else is physical (stops after
  // its first complete response, or request.maxResponses if set).
  //
  // A "response pending" reply (isResponsePending()) never counts as a
  // complete response and is never reported: the responder is re-armed for
  // its real answer and the deadline extended by kResponsePendingTimeoutMs.
  //
  // Frames already queued on the port when a request starts are discarded
  // (ICanPort::discardPending()) -- they can only be late replies to an
  // earlier request.
  //
  // Timing, as on an ELM327: the first response is awaited for the full
  // responseTimeoutMs; after each accepted frame the timer restarts. With
  // adaptive timing (and once some request has seen responses) the restart
  // is shortened to what responders have recently taken -- see
  // deadlineAfterFrame(). Collection never ends while a multi-frame
  // response is still arriving; that responder's own frame timeout governs.
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

  // Abandons the in-flight transaction (the client interrupted it). Nothing
  // further is transmitted; finished() becomes true with outcome NoData.
  void abort();

 private:
  void beginAttempt(can::Milliseconds now, can::obd::ObdCanProtocol protocol);
  void advanceTransmitPhase(can::Milliseconds now);
  void routeFrame(can::Milliseconds now, const can::CanFrame& frame);
  bool acceptFrame(uint32_t id) const;
  void finalizeCollection(can::Milliseconds now, size_t completeCount);
  void advanceAutoSearch(can::Milliseconds now);
  void finishWithOutcome(DiagnosticOutcome outcome);
  can::Milliseconds deadlineAfterFrame(can::Milliseconds now) const;
  void updateLatencyEstimate();

  can::ICanPort& port_;
  FirstSendRecorder requestRecorder_;  // what the transmitter sends through
  DiagnosticRequest request_;

  bool autoSearchActive_ = false;
  size_t autoSearchIndex_ = 0;
  can::obd::ObdCanProtocol currentProtocol_ = can::obd::ObdCanProtocol::Iso15765_11bit_500k;
  size_t effectiveMaxResponses_ = 1;

  std::optional<isotp::IsoTpTransmitter> transmitter_;
  bool transmitting_ = true;
  can::Milliseconds txCompleteTime_ = 0;
  can::Milliseconds responseDeadline_ = 0;
  can::Milliseconds pendingDeadline_ = 0;  // only while a responder is pending

  // Adaptive timing: how long after TX the slowest responder started
  // answering, this request and smoothed over previous ones.
  can::Milliseconds slowestResponderStartMs_ = 0;
  std::optional<can::Milliseconds> latencyEstimateMs_;

  std::array<std::optional<isotp::IsoTpReceiver>, kMaxResponders> receivers_;
  std::array<uint32_t, kMaxResponders> receiverSourceIds_{};
  std::array<bool, kMaxResponders> responsePending_{};
  size_t activeReceiverCount_ = 0;

  bool finished_ = false;
  DiagnosticResult result_;
};

}  // namespace esp_obd::diagnostic
