#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <initializer_list>

#include "can/can_frame.h"

// Readable construction of valid `esp_obd::can::CanFrame` test data,
// mirroring the builder style shown in docs/ARCHITECTURE.md's
// testing-strategy example:
//
//   fakeCan.queueRx(CanFrameBuilder::standard(0x7E8)
//                       .data({0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}));
//
// This builder is for valid test fixtures only. To test rejection of an
// invalid id/dlc, call esp_obd::can::makeStandardFrame (etc.) directly and
// inspect the std::optional -- don't route that case through this builder.
class CanFrameBuilder {
 public:
  static CanFrameBuilder standard(uint32_t id) {
    CanFrameBuilder builder;
    builder.id_ = id;
    builder.extended_ = false;
    return builder;
  }

  static CanFrameBuilder extended(uint32_t id) {
    CanFrameBuilder builder;
    builder.id_ = id;
    builder.extended_ = true;
    return builder;
  }

  CanFrameBuilder& data(std::initializer_list<uint8_t> bytes) {
    assert(bytes.size() <= data_.size());
    dlc_ = static_cast<uint8_t>(bytes.size());
    data_.fill(0);
    size_t i = 0;
    for (uint8_t b : bytes) {
      data_[i++] = b;
    }
    remoteRequest_ = false;
    return *this;
  }

  CanFrameBuilder& remoteRequest(uint8_t dlc = 0) {
    remoteRequest_ = true;
    dlc_ = dlc;
    return *this;
  }

  esp_obd::can::CanFrame build() const {
    auto result =
        remoteRequest_
            ? (extended_ ? esp_obd::can::makeExtendedRemoteFrame(id_, dlc_)
                         : esp_obd::can::makeStandardRemoteFrame(id_, dlc_))
            : (extended_ ? esp_obd::can::makeExtendedFrame(id_, data_, dlc_)
                         : esp_obd::can::makeStandardFrame(id_, data_, dlc_));
    assert(result.has_value() && "CanFrameBuilder given an invalid id/dlc/payload");
    return *result;
  }

  operator esp_obd::can::CanFrame() const { return build(); }

 private:
  uint32_t id_ = 0;
  bool extended_ = false;
  bool remoteRequest_ = false;
  uint8_t dlc_ = 0;
  std::array<uint8_t, 8> data_{};
};
