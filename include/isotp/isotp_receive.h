#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "can/can_frame.h"
#include "can/i_can_port.h"
#include "isotp/isotp_pci.h"

// Single-responder ISO-TP receive state machine. Advances only through
// start()/onFrame()/poll(now) -- never delay(). The caller (the future
// diagnostic/ layer, T06) is responsible for CAN-id acceptance filtering
// before handing a frame to start()/onFrame(); this class only interprets
// PCI and (if configured) the ISO-TP extended-addressing byte.
//
// See docs/ARCHITECTURE.md:
//   RxIdle -> ReceiveSingleFrame -> Complete
//   RxIdle -> ReceiveFirstFrame -> SendFlowControl -> ReceiveConsecutiveFrames -> Complete
//   Any state -> TimedOut | BusError | ProtocolError

namespace esp_obd::isotp {

enum class RxState {
  Idle,
  ReceivingConsecutiveFrames,
  Complete,
  TimedOut,
  BusError,
  ProtocolError,
};

// kMaxPayloadBytes is shared with the TX side; see isotp_pci.h. 40 raw
// frames covers 255 bytes worth of 7-byte Consecutive Frames (37) plus the
// First Frame and slack.
inline constexpr size_t kMaxRawFrames = 40;

struct RxConfig {
  // ISO-TP extended addressing (ATCEA/ATCEAhh/ATCERhh): an extra address
  // byte prefixed to every frame's payload. This is unrelated to whether
  // the CAN id itself is 11-bit or 29-bit.
  bool extendedAddressingEnabled = false;
  uint8_t requiredExtendedAddressByte = 0;  // ATCERhh: required in received frames
  uint8_t transmitExtendedAddressByte = 0;  // ATCEAhh: prefixed onto our own FC frame

  // ATCFC0/ATCFC1: whether we send Flow Control automatically after a
  // First Frame. T09 will extend this for ATFCSM's manual-header/
  // manual-data modes; this only implements the simple on/off case.
  bool sendAutomaticFlowControl = true;

  can::Milliseconds frameTimeoutMs = 200;  // max gap before the next expected frame

  uint32_t flowControlId = 0;
  bool flowControlIdIsExtendedCan = false;
  uint8_t flowControlBlockSize = 0;  // 0 = no limit; else re-arm FC every N CFs
  uint8_t flowControlStMin = 0;      // minimum separation we ask the sender for
};

class IsoTpReceiver {
 public:
  IsoTpReceiver(can::ICanPort& port, const RxConfig& config) : port_(port), config_(config) {}

  // Begins a new reception with the first accepted frame (Single or First
  // Frame). Fully resets prior transaction state, so one instance may be
  // reused across requests. A frame whose extended-address byte doesn't
  // match is silently ignored (state stays Idle), per
  // docs/ELM_COMMAND_BEHAVIOR.md's ATCERhh row -- it is not a protocol
  // error.
  void start(can::Milliseconds now, const can::CanFrame& frame);

  // Feeds a subsequent frame (expected: Consecutive Frame) while
  // state() == ReceivingConsecutiveFrames. Also silently ignores an
  // extended-address mismatch.
  void onFrame(can::Milliseconds now, const can::CanFrame& frame);

  // Advances timeout tracking. Call every poll tick even when no frame
  // arrived; never blocks or sleeps.
  void poll(can::Milliseconds now);

  RxState state() const { return state_; }

  // Valid once state() == Complete: the reassembled ISO-TP payload, PCI and
  // padding already stripped.
  const uint8_t* payload() const { return payload_.data(); }
  size_t payloadLength() const { return payloadReceived_; }

  // Every raw CAN frame of this transaction, for ATH1 header-on display.
  const can::CanFrame* rawFrames() const { return rawFrames_.data(); }
  size_t rawFrameCount() const { return rawFrameCount_; }

 private:
  // Returns the PCI window (offset into frame.data, and its length) after
  // stripping the extended-addressing byte if configured; returns false if
  // an enabled extended address byte doesn't match (caller should ignore
  // the frame, not fail it).
  bool resolveWindow(const can::CanFrame& frame, const uint8_t** windowData, size_t* windowLen) const;

  void recordRawFrame(const can::CanFrame& frame);
  void sendFlowControl(FlowStatus status);

  can::ICanPort& port_;
  RxConfig config_;
  RxState state_ = RxState::Idle;

  std::array<uint8_t, kMaxPayloadBytes> payload_{};
  size_t payloadDeclaredLength_ = 0;
  size_t payloadReceived_ = 0;
  uint8_t expectedSequence_ = 1;
  uint8_t cfsSinceFlowControl_ = 0;

  std::array<can::CanFrame, kMaxRawFrames> rawFrames_{};
  size_t rawFrameCount_ = 0;

  can::Milliseconds deadline_ = 0;
};

}  // namespace esp_obd::isotp
