#pragma once

#include <cstddef>
#include <cstdint>

#include "can/can_frame.h"
#include "can/i_can_port.h"
#include "isotp/isotp_pci.h"

// Single-request ISO-TP transmit state machine. Advances only through
// start()/onFlowControl()/poll(now) -- never delay(). CAF0 (raw mode) does
// not go through this class at all: it never interprets ISO-TP, so the
// diagnostic layer (T06) should call can::makeStandardFrame/
// makeExtendedFrame directly with the caller-supplied bytes for that case.
//
// See docs/ARCHITECTURE.md:
//   TxIdle -> SendSingleFrame -> Complete
//   TxIdle -> SendFirstFrame -> WaitForFlowControl -> SendConsecutiveFrames -> Complete
//   Any state -> TimedOut | BusError | ProtocolError

namespace esp_obd::isotp {

enum class TxState {
  Idle,
  WaitingForFlowControl,
  SendingConsecutiveFrames,
  Complete,
  TimedOut,
  BusError,
  ProtocolError,  // malformed/unrecognized Flow Control
  Overflow,       // Flow Control reported Overflow/Abort
};

struct TxConfig {
  uint32_t id = 0;
  bool idIsExtendedCan = false;

  // ISO-TP extended addressing (ATCEA/ATCEAhh/ATCERhh): an extra address
  // byte prefixed to every frame's payload, unrelated to the CAN id width.
  bool extendedAddressingEnabled = false;
  uint8_t transmitExtendedAddressByte = 0;  // ATCEAhh: prefixed onto our SF/FF/CF frames
  uint8_t requiredExtendedAddressByte = 0;  // ATCERhh: required on an incoming FC

  can::Milliseconds sendTimeoutMs = 100;         // per-frame ICanPort::send() timeout
  can::Milliseconds flowControlTimeoutMs = 200;  // max wait for an FC (initial, or after a block)
};

class IsoTpTransmitter {
 public:
  IsoTpTransmitter(can::ICanPort& port, const TxConfig& config) : port_(port), config_(config) {}

  // Begins sending `payload` (caller-owned; must stay valid and unchanged
  // for the lifetime of this transaction -- this class never copies it).
  // payloadLen <= 7 sends one Single Frame and completes within this call;
  // longer payloads send the First Frame and move to
  // WaitingForFlowControl. payloadLen == 0 or > kMaxPayloadBytes is a
  // caller error and yields ProtocolError.
  void start(can::Milliseconds now, const uint8_t* payload, size_t payloadLen);

  // Feeds an incoming Flow Control frame while
  // state() == WaitingForFlowControl. A frame with a mismatched extended
  // address is silently ignored, matching the RX side's ATCERhh handling.
  void onFlowControl(can::Milliseconds now, const can::CanFrame& frame);

  // Advances timeout tracking and paces outgoing Consecutive Frames
  // according to the block size/STmin the last Flow Control specified.
  // Never blocks or sleeps; call every loop tick.
  void poll(can::Milliseconds now);

  TxState state() const { return state_; }
  size_t bytesSent() const { return bytesSent_; }

 private:
  size_t frameCapacity() const {  // bytes of payload per frame, after PCI (+ ext. addr)
    return 8 - (config_.extendedAddressingEnabled ? 1 : 0);
  }

  void sendSingleFrame(can::Milliseconds now, size_t payloadLen);
  void sendFirstFrame(can::Milliseconds now);
  void sendNextConsecutiveFrame(can::Milliseconds now);

  can::ICanPort& port_;
  TxConfig config_;
  TxState state_ = TxState::Idle;

  const uint8_t* payload_ = nullptr;
  size_t payloadLen_ = 0;
  size_t bytesSent_ = 0;
  uint8_t nextSequence_ = 1;

  uint8_t blockSize_ = 0;   // 0 = no limit, from the last Flow Control
  uint8_t cfsSentInBlock_ = 0;
  can::Milliseconds stMinMs_ = 0;       // from the last Flow Control
  can::Milliseconds nextSendTime_ = 0;  // earliest time the next CF may go out
  can::Milliseconds deadline_ = 0;      // for WaitingForFlowControl timeout
};

}  // namespace esp_obd::isotp
