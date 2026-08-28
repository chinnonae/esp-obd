#include <unity.h>

#include <array>
#include <vector>

#include "../../support/fake_can_port.h"
#include "isotp/isotp_transmit.h"

using namespace esp_obd::can;
using namespace esp_obd::isotp;

void setUp() {}
void tearDown() {}

namespace {

TxConfig defaultConfig() {
  TxConfig config;
  config.id = 0x7DF;
  config.sendTimeoutMs = 50;
  config.flowControlTimeoutMs = 200;
  return config;
}

CanFrame flowControlFrame(uint32_t id, uint8_t statusNibble, uint8_t blockSize, uint8_t stMin) {
  std::array<uint8_t, 8> data{};
  data[0] = static_cast<uint8_t>(0x30 | (statusNibble & 0x0F));
  data[1] = blockSize;
  data[2] = stMin;
  return *makeStandardFrame(id, data, 8);
}

}  // namespace

// --- Single frame -----------------------------------------------------------

void test_010c_produces_exact_standard_obd_frame() {
  FakeCanPort port;
  IsoTpTransmitter tx(port, defaultConfig());
  const uint8_t payload[] = {0x01, 0x0C};

  tx.start(0, payload, 2);

  TEST_ASSERT_EQUAL(static_cast<int>(TxState::Complete), static_cast<int>(tx.state()));
  TEST_ASSERT_EQUAL(1, port.transmitted().size());
  const CanFrame& frame = port.transmitted()[0];
  TEST_ASSERT_EQUAL_UINT32(0x7DF, frame.id);
  TEST_ASSERT_FALSE(frame.extended);
  const uint8_t expected[] = {0x02, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, frame.data.data(), 8);
}

void test_single_frame_one_byte_payload() {
  FakeCanPort port;
  IsoTpTransmitter tx(port, defaultConfig());
  const uint8_t payload[] = {0x3E};

  tx.start(0, payload, 1);

  TEST_ASSERT_EQUAL(static_cast<int>(TxState::Complete), static_cast<int>(tx.state()));
  const CanFrame& frame = port.transmitted()[0];
  TEST_ASSERT_EQUAL_UINT8(0x01, frame.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0x3E, frame.data[1]);
}

void test_single_frame_seven_byte_payload() {
  FakeCanPort port;
  IsoTpTransmitter tx(port, defaultConfig());
  const uint8_t payload[] = {1, 2, 3, 4, 5, 6, 7};

  tx.start(0, payload, 7);

  TEST_ASSERT_EQUAL(static_cast<int>(TxState::Complete), static_cast<int>(tx.state()));
  const CanFrame& frame = port.transmitted()[0];
  TEST_ASSERT_EQUAL_UINT8(0x07, frame.data[0]);
  const uint8_t expected[] = {0x07, 1, 2, 3, 4, 5, 6, 7};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, frame.data.data(), 8);
}

// --- Multi-frame: FF/CF sequence, wrap, and completion -----------------------

void test_eight_byte_payload_uses_first_frame_then_one_cf() {
  FakeCanPort port;
  IsoTpTransmitter tx(port, defaultConfig());
  const uint8_t payload[] = {1, 2, 3, 4, 5, 6, 7, 8};

  tx.start(0, payload, 8);
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::WaitingForFlowControl), static_cast<int>(tx.state()));
  TEST_ASSERT_EQUAL(1, port.transmitted().size());
  const uint8_t expectedFf[] = {0x10, 0x08, 1, 2, 3, 4, 5, 6};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedFf, port.transmitted()[0].data.data(), 8);

  tx.onFlowControl(10, flowControlFrame(0x7E8, 0 /*ContinueToSend*/, 0, 0));
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::SendingConsecutiveFrames),
                     static_cast<int>(tx.state()));

  tx.poll(10);  // nextSendTime_ == 10 (set at FC time): sends immediately
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::Complete), static_cast<int>(tx.state()));
  TEST_ASSERT_EQUAL(2, port.transmitted().size());
  const uint8_t expectedCf[] = {0x21, 7, 8, 0, 0, 0, 0, 0};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedCf, port.transmitted()[1].data.data(), 8);
}

void test_multi_frame_sequence_wraps_from_15_to_0() {
  FakeCanPort port;
  TxConfig config = defaultConfig();
  IsoTpTransmitter tx(port, config);

  const size_t total = 6 + 16 * 7;  // FF(6) + 16 CFs: sequence wraps 15 -> 0
  std::vector<uint8_t> payload(total);
  for (size_t i = 0; i < total; ++i) payload[i] = static_cast<uint8_t>(i);

  tx.start(0, payload.data(), payload.size());
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::WaitingForFlowControl), static_cast<int>(tx.state()));

  tx.onFlowControl(0, flowControlFrame(0x7E8, 0, /*blockSize=*/0, /*stMin=*/0));
  Milliseconds now = 0;
  for (int i = 0; i < 16; ++i) {
    tx.poll(now++);
  }

  TEST_ASSERT_EQUAL(static_cast<int>(TxState::Complete), static_cast<int>(tx.state()));
  TEST_ASSERT_EQUAL(total, tx.bytesSent());
  TEST_ASSERT_EQUAL(17, port.transmitted().size());  // 1 FF + 16 CF

  // First CF (index 1) has sequence 1; the 16th CF (index 16) wraps to 0.
  TEST_ASSERT_EQUAL_UINT8(0x21, port.transmitted()[1].data[0]);
  TEST_ASSERT_EQUAL_UINT8(0x20, port.transmitted()[16].data[0]);  // seq 0, wrapped
}

void test_flow_control_reissued_after_block_size_cfs() {
  FakeCanPort port;
  IsoTpTransmitter tx(port, defaultConfig());

  const uint8_t payload[6 + 3 * 7] = {0};  // FF(6) + 3 CFs
  tx.start(0, payload, sizeof(payload));

  tx.onFlowControl(0, flowControlFrame(0x7E8, 0, /*blockSize=*/2, /*stMin=*/0));
  tx.poll(0);  // CF1
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::SendingConsecutiveFrames),
                     static_cast<int>(tx.state()));
  tx.poll(0);  // CF2: block size reached, now waits for another FC
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::WaitingForFlowControl), static_cast<int>(tx.state()));
  TEST_ASSERT_EQUAL(3, port.transmitted().size());  // FF + CF1 + CF2

  tx.onFlowControl(0, flowControlFrame(0x7E8, 0, /*blockSize=*/2, /*stMin=*/0));
  tx.poll(0);  // CF3: completes
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::Complete), static_cast<int>(tx.state()));
  TEST_ASSERT_EQUAL(4, port.transmitted().size());
}

// --- Flow control status handling --------------------------------------------

void test_fc_wait_status_delays_without_erroring() {
  FakeCanPort port;
  TxConfig config = defaultConfig();
  config.flowControlTimeoutMs = 100;
  IsoTpTransmitter tx(port, config);
  const uint8_t payload[8] = {0};

  tx.start(0, payload, sizeof(payload));
  tx.onFlowControl(0, flowControlFrame(0x7E8, 1 /*Wait*/, 0, 0));
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::WaitingForFlowControl), static_cast<int>(tx.state()));

  tx.poll(90);  // within the refreshed deadline: still waiting, not timed out
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::WaitingForFlowControl), static_cast<int>(tx.state()));

  tx.onFlowControl(90, flowControlFrame(0x7E8, 0 /*ContinueToSend*/, 0, 0));
  tx.poll(90);
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::Complete), static_cast<int>(tx.state()));
}

void test_fc_overflow_status_aborts() {
  FakeCanPort port;
  IsoTpTransmitter tx(port, defaultConfig());
  const uint8_t payload[8] = {0};

  tx.start(0, payload, sizeof(payload));
  tx.onFlowControl(0, flowControlFrame(0x7E8, 2 /*Overflow*/, 0, 0));

  TEST_ASSERT_EQUAL(static_cast<int>(TxState::Overflow), static_cast<int>(tx.state()));
}

void test_missing_fc_times_out() {
  FakeCanPort port;
  TxConfig config = defaultConfig();
  config.flowControlTimeoutMs = 100;
  IsoTpTransmitter tx(port, config);
  const uint8_t payload[8] = {0};

  tx.start(0, payload, sizeof(payload));
  tx.poll(99);
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::WaitingForFlowControl), static_cast<int>(tx.state()));
  tx.poll(100);
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::TimedOut), static_cast<int>(tx.state()));
}

void test_malformed_fc_is_protocol_error() {
  FakeCanPort port;
  IsoTpTransmitter tx(port, defaultConfig());
  const uint8_t payload[8] = {0};

  tx.start(0, payload, sizeof(payload));
  // A Single Frame PCI where a Flow Control was expected.
  std::array<uint8_t, 8> data{0x03, 0, 0, 0, 0, 0, 0, 0};
  tx.onFlowControl(0, *makeStandardFrame(0x7E8, data, 8));

  TEST_ASSERT_EQUAL(static_cast<int>(TxState::ProtocolError), static_cast<int>(tx.state()));
}

// --- STmin pacing, testable without real time --------------------------------

void test_stmin_paces_between_consecutive_frames() {
  FakeCanPort port;
  IsoTpTransmitter tx(port, defaultConfig());

  const uint8_t payload[6 + 3 * 7] = {0};  // 3 CFs
  tx.start(0, payload, sizeof(payload));
  tx.onFlowControl(0, flowControlFrame(0x7E8, 0, /*blockSize=*/0, /*stMin=*/50));

  tx.poll(0);  // CF1 sent immediately
  TEST_ASSERT_EQUAL(2, port.transmitted().size());

  tx.poll(10);  // STmin not yet elapsed
  TEST_ASSERT_EQUAL(2, port.transmitted().size());

  tx.poll(50);  // exactly STmin later: CF2 sent
  TEST_ASSERT_EQUAL(3, port.transmitted().size());

  tx.poll(60);  // not yet another 50ms
  TEST_ASSERT_EQUAL(3, port.transmitted().size());

  tx.poll(100);  // CF3 sent, transaction completes
  TEST_ASSERT_EQUAL(4, port.transmitted().size());
  TEST_ASSERT_EQUAL(static_cast<int>(TxState::Complete), static_cast<int>(tx.state()));
}

// --- Bus failure --------------------------------------------------------------

void test_send_failure_yields_bus_error() {
  FakeCanPort port;
  port.setNextSendResult(CanResult::BusError);
  IsoTpTransmitter tx(port, defaultConfig());
  const uint8_t payload[] = {0x01, 0x0C};

  tx.start(0, payload, 2);

  TEST_ASSERT_EQUAL(static_cast<int>(TxState::BusError), static_cast<int>(tx.state()));
}

// --- Extended addressing and extended (29-bit) CAN id ------------------------

void test_extended_addressing_prefixes_every_frame() {
  FakeCanPort port;
  TxConfig config = defaultConfig();
  config.extendedAddressingEnabled = true;
  config.transmitExtendedAddressByte = 0xAA;
  IsoTpTransmitter tx(port, config);
  const uint8_t payload[] = {0x01, 0x0C};

  tx.start(0, payload, 2);

  const CanFrame& frame = port.transmitted()[0];
  TEST_ASSERT_EQUAL_UINT8(0xAA, frame.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0x02, frame.data[1]);  // PCI now at offset 1
  TEST_ASSERT_EQUAL_UINT8(0x01, frame.data[2]);
}

void test_extended_can_id_used_for_transmitted_frames() {
  FakeCanPort port;
  TxConfig config = defaultConfig();
  config.id = 0x18DB33F1;
  config.idIsExtendedCan = true;
  IsoTpTransmitter tx(port, config);
  const uint8_t payload[] = {0x01, 0x0C};

  tx.start(0, payload, 2);

  const CanFrame& frame = port.transmitted()[0];
  TEST_ASSERT_EQUAL_UINT32(0x18DB33F1, frame.id);
  TEST_ASSERT_TRUE(frame.extended);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_010c_produces_exact_standard_obd_frame);
  RUN_TEST(test_single_frame_one_byte_payload);
  RUN_TEST(test_single_frame_seven_byte_payload);
  RUN_TEST(test_eight_byte_payload_uses_first_frame_then_one_cf);
  RUN_TEST(test_multi_frame_sequence_wraps_from_15_to_0);
  RUN_TEST(test_flow_control_reissued_after_block_size_cfs);
  RUN_TEST(test_fc_wait_status_delays_without_erroring);
  RUN_TEST(test_fc_overflow_status_aborts);
  RUN_TEST(test_missing_fc_times_out);
  RUN_TEST(test_malformed_fc_is_protocol_error);
  RUN_TEST(test_stmin_paces_between_consecutive_frames);
  RUN_TEST(test_send_failure_yields_bus_error);
  RUN_TEST(test_extended_addressing_prefixes_every_frame);
  RUN_TEST(test_extended_can_id_used_for_transmitted_frames);
  return UNITY_END();
}
