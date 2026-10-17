#include <unity.h>

#include <array>

#include "../../support/fake_can_port.h"
#include "../../support/fake_clock.h"
#include "isotp/isotp_receive.h"

using namespace esp_obd::can;
using namespace esp_obd::isotp;

void setUp() {}
void tearDown() {}

namespace {

RxConfig defaultConfig() {
  RxConfig config;
  config.flowControlId = 0x7E0;
  config.frameTimeoutMs = 200;
  return config;
}

CanFrame singleFrame(uint32_t id, std::array<uint8_t, 8> data) {
  return *makeStandardFrame(id, data, 8);
}

CanFrame firstFrame(uint32_t id, uint16_t declaredLength, std::array<uint8_t, 6> firstSix,
                    bool extended = false) {
  std::array<uint8_t, 8> data{};
  data[0] = static_cast<uint8_t>(0x10 | ((declaredLength >> 8) & 0x0F));
  data[1] = static_cast<uint8_t>(declaredLength & 0xFF);
  for (size_t i = 0; i < 6; ++i) data[2 + i] = firstSix[i];
  return extended ? *makeExtendedFrame(id, data, 8) : *makeStandardFrame(id, data, 8);
}

CanFrame consecutiveFrame(uint32_t id, uint8_t sequence, std::array<uint8_t, 7> bytes) {
  std::array<uint8_t, 8> data{};
  data[0] = static_cast<uint8_t>(0x20 | (sequence & 0x0F));
  for (size_t i = 0; i < 7; ++i) data[1 + i] = bytes[i];
  return *makeStandardFrame(id, data, 8);
}

}  // namespace

// --- Single frame ---------------------------------------------------------

void test_single_frame_reassembles_and_strips_padding() {
  FakeCanPort port;
  IsoTpReceiver rx(port, defaultConfig());

  rx.start(0, singleFrame(0x7E8, {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}));

  TEST_ASSERT_EQUAL(static_cast<int>(RxState::Complete), static_cast<int>(rx.state()));
  TEST_ASSERT_EQUAL(6, rx.payloadLength());
  const uint8_t expected[] = {0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, rx.payload(), 6);
  TEST_ASSERT_EQUAL(0, port.transmitted().size());  // no FC for a single frame
}

// --- Multi-frame happy path ------------------------------------------------

void test_multi_frame_happy_path_reassembles_exactly() {
  FakeCanPort port;
  RxConfig config = defaultConfig();
  IsoTpReceiver rx(port, config);

  // 20-byte payload: bytes 0..19, split FF(6) + CF(7) + CF(7).
  rx.start(0, firstFrame(0x7E8, 20, {0, 1, 2, 3, 4, 5}));
  TEST_ASSERT_EQUAL(static_cast<int>(RxState::ReceivingConsecutiveFrames),
                     static_cast<int>(rx.state()));
  TEST_ASSERT_EQUAL(1, port.transmitted().size());  // automatic FC sent

  rx.onFrame(10, consecutiveFrame(0x7E8, 1, {6, 7, 8, 9, 10, 11, 12}));
  TEST_ASSERT_EQUAL(static_cast<int>(RxState::ReceivingConsecutiveFrames),
                     static_cast<int>(rx.state()));

  rx.onFrame(20, consecutiveFrame(0x7E8, 2, {13, 14, 15, 16, 17, 18, 19}));
  TEST_ASSERT_EQUAL(static_cast<int>(RxState::Complete), static_cast<int>(rx.state()));

  TEST_ASSERT_EQUAL(20, rx.payloadLength());
  for (uint8_t i = 0; i < 20; ++i) {
    TEST_ASSERT_EQUAL_UINT8(i, rx.payload()[i]);
  }
  TEST_ASSERT_EQUAL(3, rx.rawFrameCount());  // FF + 2 CFs, for ATH1 display
}

void test_sequence_wraps_from_15_to_0() {
  FakeCanPort port;
  IsoTpReceiver rx(port, defaultConfig());

  // 6 (FF) + 16 * 7 = 118 bytes; the 16th CF's sequence wraps 15 -> 0.
  const uint16_t total = 118;
  std::array<uint8_t, 6> ffBytes{};
  for (size_t i = 0; i < 6; ++i) ffBytes[i] = static_cast<uint8_t>(i);
  rx.start(0, firstFrame(0x7E8, total, ffBytes));

  uint8_t nextByte = 6;
  for (int cfIndex = 1; cfIndex <= 16; ++cfIndex) {
    uint8_t seq = static_cast<uint8_t>(cfIndex % 16);  // 1..15, then 0
    std::array<uint8_t, 7> bytes{};
    for (size_t i = 0; i < 7; ++i) bytes[i] = nextByte++;
    rx.onFrame(cfIndex * 10, consecutiveFrame(0x7E8, seq, bytes));
  }

  TEST_ASSERT_EQUAL(static_cast<int>(RxState::Complete), static_cast<int>(rx.state()));
  TEST_ASSERT_EQUAL(total, rx.payloadLength());
  for (uint16_t i = 0; i < total; ++i) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(i), rx.payload()[i]);
  }
}

void test_missing_cf_is_a_protocol_error_not_a_partial_success() {
  FakeCanPort port;
  IsoTpReceiver rx(port, defaultConfig());

  rx.start(0, firstFrame(0x7E8, 20, {0, 1, 2, 3, 4, 5}));
  // Skip sequence 1 entirely; jump straight to sequence 2.
  rx.onFrame(10, consecutiveFrame(0x7E8, 2, {13, 14, 15, 16, 17, 18, 19}));

  TEST_ASSERT_EQUAL(static_cast<int>(RxState::ProtocolError), static_cast<int>(rx.state()));
}

void test_wrong_sequence_is_a_protocol_error() {
  FakeCanPort port;
  IsoTpReceiver rx(port, defaultConfig());

  rx.start(0, firstFrame(0x7E8, 20, {0, 1, 2, 3, 4, 5}));
  rx.onFrame(10, consecutiveFrame(0x7E8, 5, {6, 7, 8, 9, 10, 11, 12}));  // expected 1, got 5

  TEST_ASSERT_EQUAL(static_cast<int>(RxState::ProtocolError), static_cast<int>(rx.state()));
}

void test_declared_length_larger_than_limit_is_rejected() {
  FakeCanPort port;
  IsoTpReceiver rx(port, defaultConfig());

  rx.start(0, firstFrame(0x7E8, kMaxPayloadBytes + 1, {0, 1, 2, 3, 4, 5}));

  TEST_ASSERT_EQUAL(static_cast<int>(RxState::ProtocolError), static_cast<int>(rx.state()));
  TEST_ASSERT_EQUAL(0, port.transmitted().size());  // no FC sent for a rejected FF
}

// --- Timeouts (deterministic under FakeClock) ------------------------------

void test_timeout_waiting_for_first_cf() {
  FakeCanPort port;
  RxConfig config = defaultConfig();
  config.frameTimeoutMs = 100;
  IsoTpReceiver rx(port, config);

  rx.start(0, firstFrame(0x7E8, 20, {0, 1, 2, 3, 4, 5}));
  rx.poll(99);
  TEST_ASSERT_EQUAL(static_cast<int>(RxState::ReceivingConsecutiveFrames),
                     static_cast<int>(rx.state()));
  rx.poll(100);
  TEST_ASSERT_EQUAL(static_cast<int>(RxState::TimedOut), static_cast<int>(rx.state()));
}

void test_timeout_between_consecutive_frames_after_partial_response() {
  FakeCanPort port;
  RxConfig config = defaultConfig();
  config.frameTimeoutMs = 100;
  IsoTpReceiver rx(port, config);

  rx.start(0, firstFrame(0x7E8, 20, {0, 1, 2, 3, 4, 5}));
  rx.onFrame(10, consecutiveFrame(0x7E8, 1, {6, 7, 8, 9, 10, 11, 12}));
  // Never send the second CF.
  rx.poll(10 + 100);
  TEST_ASSERT_EQUAL(static_cast<int>(RxState::TimedOut), static_cast<int>(rx.state()));
  // Only the bytes actually received (FF + 1 CF) are retained; a timeout
  // must never be reported as a successful, if truncated, payload.
  TEST_ASSERT_EQUAL(13, rx.payloadLength());
}

// --- Extended addressing (ATCEA/ATCEAhh/ATCERhh) ---------------------------

void test_extended_address_mismatch_is_silently_ignored() {
  FakeCanPort port;
  RxConfig config = defaultConfig();
  config.extendedAddressingEnabled = true;
  config.requiredExtendedAddressByte = 0x01;
  IsoTpReceiver rx(port, config);

  // First byte 0x02 != required 0x01: ignored, no state change.
  std::array<uint8_t, 8> data{0x02, 0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10};
  rx.start(0, *makeStandardFrame(0x7E8, data, 8));

  TEST_ASSERT_EQUAL(static_cast<int>(RxState::Idle), static_cast<int>(rx.state()));
}

void test_extended_address_match_is_processed() {
  FakeCanPort port;
  RxConfig config = defaultConfig();
  config.extendedAddressingEnabled = true;
  config.requiredExtendedAddressByte = 0x01;
  config.transmitExtendedAddressByte = 0xAA;
  IsoTpReceiver rx(port, config);

  std::array<uint8_t, 8> data{0x01, 0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10};
  rx.start(0, *makeStandardFrame(0x7E8, data, 8));

  TEST_ASSERT_EQUAL(static_cast<int>(RxState::Complete), static_cast<int>(rx.state()));
  const uint8_t expected[] = {0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, rx.payload(), 6);
}

void test_flow_control_frame_prefixes_transmit_extended_address_byte() {
  FakeCanPort port;
  RxConfig config = defaultConfig();
  config.extendedAddressingEnabled = true;
  config.requiredExtendedAddressByte = 0x01;
  config.transmitExtendedAddressByte = 0xAA;
  IsoTpReceiver rx(port, config);

  std::array<uint8_t, 8> ff{0x01, 0x10, 0x14, 0, 1, 2, 3, 4};  // declared length 20
  rx.start(0, *makeStandardFrame(0x7E8, ff, 8));

  TEST_ASSERT_EQUAL(1, port.transmitted().size());
  const CanFrame& fc = port.transmitted()[0];
  TEST_ASSERT_EQUAL_UINT8(0xAA, fc.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0x30, fc.data[1]);  // PCI: Flow Control, ContinueToSend
}

// --- Flow control ID/data correctness (11-bit and 29-bit) ------------------

void test_flow_control_frame_standard_id_and_no_limits() {
  FakeCanPort port;
  RxConfig config = defaultConfig();
  config.flowControlId = 0x7E0;
  config.flowControlIdIsExtendedCan = false;
  IsoTpReceiver rx(port, config);

  rx.start(0, firstFrame(0x7E8, 20, {0, 1, 2, 3, 4, 5}));

  TEST_ASSERT_EQUAL(1, port.transmitted().size());
  const CanFrame& fc = port.transmitted()[0];
  TEST_ASSERT_EQUAL_UINT32(0x7E0, fc.id);
  TEST_ASSERT_FALSE(fc.extended);
  TEST_ASSERT_EQUAL_UINT8(0x30, fc.data[0]);  // ContinueToSend, no extended address byte
  TEST_ASSERT_EQUAL_UINT8(0, fc.data[1]);     // block size 0 = no limit
  TEST_ASSERT_EQUAL_UINT8(0, fc.data[2]);     // STmin 0
}

void test_flow_control_frame_extended_can_id() {
  FakeCanPort port;
  RxConfig config = defaultConfig();
  config.flowControlId = 0x18DAF110;
  config.flowControlIdIsExtendedCan = true;
  IsoTpReceiver rx(port, config);

  rx.start(0, firstFrame(0x18DA10F1, 20, {0, 1, 2, 3, 4, 5}, /*extended=*/true));

  TEST_ASSERT_EQUAL(1, port.transmitted().size());
  const CanFrame& fc = port.transmitted()[0];
  TEST_ASSERT_EQUAL_UINT32(0x18DAF110, fc.id);
  TEST_ASSERT_TRUE(fc.extended);
}

void test_flow_control_reissued_after_block_size_cfs() {
  FakeCanPort port;
  RxConfig config = defaultConfig();
  config.flowControlBlockSize = 2;
  IsoTpReceiver rx(port, config);

  // 6 (FF) + 3*7 = 27 bytes, needing 3 CFs; block size 2 re-arms FC after
  // the 2nd CF, with a 3rd CF still remaining.
  rx.start(0, firstFrame(0x7E8, 27, {0, 1, 2, 3, 4, 5}));
  TEST_ASSERT_EQUAL(1, port.transmitted().size());  // initial FC

  rx.onFrame(10, consecutiveFrame(0x7E8, 1, {6, 7, 8, 9, 10, 11, 12}));
  TEST_ASSERT_EQUAL(1, port.transmitted().size());  // not yet: only 1 CF so far

  rx.onFrame(20, consecutiveFrame(0x7E8, 2, {13, 14, 15, 16, 17, 18, 19}));
  TEST_ASSERT_EQUAL(2, port.transmitted().size());  // block size 2 reached: re-armed
  TEST_ASSERT_EQUAL(static_cast<int>(RxState::ReceivingConsecutiveFrames),
                     static_cast<int>(rx.state()));

  rx.onFrame(30, consecutiveFrame(0x7E8, 3, {20, 21, 22, 23, 24, 25, 26}));
  TEST_ASSERT_EQUAL(static_cast<int>(RxState::Complete), static_cast<int>(rx.state()));
  TEST_ASSERT_EQUAL(2, port.transmitted().size());  // no further FC once complete
}

// --- Bounded buffer: no read/write beyond configured limits ----------------

void test_raw_frame_count_never_exceeds_configured_capacity() {
  FakeCanPort port;
  IsoTpReceiver rx(port, defaultConfig());

  // Largest in-range payload: kMaxPayloadBytes, needing ceil((max-6)/7) CFs.
  const uint16_t total = static_cast<uint16_t>(kMaxPayloadBytes);
  std::array<uint8_t, 6> ffBytes{};
  rx.start(0, firstFrame(0x7E8, total, ffBytes));

  size_t received = 6;
  uint8_t seq = 1;
  Milliseconds now = 0;
  while (received < total) {
    std::array<uint8_t, 7> bytes{};
    size_t take = std::min<size_t>(7, total - received);
    rx.onFrame(now += 1, consecutiveFrame(0x7E8, seq, bytes));
    received += take;
    seq = (seq == 15) ? 0 : static_cast<uint8_t>(seq + 1);
  }

  TEST_ASSERT_EQUAL(static_cast<int>(RxState::Complete), static_cast<int>(rx.state()));
  TEST_ASSERT_TRUE(rx.rawFrameCount() <= kMaxRawFrames);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_single_frame_reassembles_and_strips_padding);
  RUN_TEST(test_multi_frame_happy_path_reassembles_exactly);
  RUN_TEST(test_sequence_wraps_from_15_to_0);
  RUN_TEST(test_missing_cf_is_a_protocol_error_not_a_partial_success);
  RUN_TEST(test_wrong_sequence_is_a_protocol_error);
  RUN_TEST(test_declared_length_larger_than_limit_is_rejected);
  RUN_TEST(test_timeout_waiting_for_first_cf);
  RUN_TEST(test_timeout_between_consecutive_frames_after_partial_response);
  RUN_TEST(test_extended_address_mismatch_is_silently_ignored);
  RUN_TEST(test_extended_address_match_is_processed);
  RUN_TEST(test_flow_control_frame_prefixes_transmit_extended_address_byte);
  RUN_TEST(test_flow_control_frame_standard_id_and_no_limits);
  RUN_TEST(test_flow_control_frame_extended_can_id);
  RUN_TEST(test_flow_control_reissued_after_block_size_cfs);
  RUN_TEST(test_raw_frame_count_never_exceeds_configured_capacity);
  return UNITY_END();
}
