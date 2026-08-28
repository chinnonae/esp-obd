#pragma once

#include <cassert>
#include <cstdint>
#include <initializer_list>

#include "fake_can_port.h"

// Readable construction of `FakeCanFrame` test data, mirroring the builder
// style shown in docs/ARCHITECTURE.md's testing-strategy example:
//
//   fakeCan.queueRx(CanFrameBuilder::standard(0x7E8)
//                       .data({0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}));
class CanFrameBuilder {
 public:
  static CanFrameBuilder standard(uint32_t id) {
    CanFrameBuilder builder;
    builder.frame_.id = id;
    builder.frame_.extended = false;
    return builder;
  }

  static CanFrameBuilder extended(uint32_t id) {
    CanFrameBuilder builder;
    builder.frame_.id = id;
    builder.frame_.extended = true;
    return builder;
  }

  CanFrameBuilder& data(std::initializer_list<uint8_t> bytes) {
    assert(bytes.size() <= frame_.data.size());
    frame_.dlc = static_cast<uint8_t>(bytes.size());
    frame_.data.fill(0);
    size_t i = 0;
    for (uint8_t b : bytes) {
      frame_.data[i++] = b;
    }
    return *this;
  }

  CanFrameBuilder& remoteRequest(bool value = true) {
    frame_.remoteRequest = value;
    return *this;
  }

  FakeCanFrame build() const { return frame_; }
  operator FakeCanFrame() const { return frame_; }

 private:
  FakeCanFrame frame_;
};
