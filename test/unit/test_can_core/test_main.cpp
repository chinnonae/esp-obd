#include <unity.h>

#include "can/can_filter.h"
#include "can/obd_addresses.h"
#include "../../support/can_frame_builder.h"
#include "../../support/fake_can_port.h"

using namespace esp_obd::can;

void setUp() {}
void tearDown() {}

// --- Frame creation boundary values -----------------------------------

void test_standard_id_boundary() {
  TEST_ASSERT_TRUE(makeStandardFrame(0x7FF, {0x01}).has_value());
  TEST_ASSERT_FALSE(makeStandardFrame(0x800, {0x01}).has_value());
}

void test_extended_id_boundary() {
  TEST_ASSERT_TRUE(makeExtendedFrame(0x1FFFFFFF, {0x01}).has_value());
  TEST_ASSERT_FALSE(makeExtendedFrame(0x20000000, {0x01}).has_value());
}

void test_dlc_boundary() {
  TEST_ASSERT_TRUE(makeStandardFrame(0x100, {}).has_value());  // dlc 0
  TEST_ASSERT_TRUE(makeStandardFrame(0x100, {0, 1, 2, 3, 4, 5, 6, 7}).has_value());  // dlc 8
  TEST_ASSERT_FALSE(makeStandardRemoteFrame(0x100, 9).has_value());  // dlc 9: rejected
}

void test_remote_frame_carries_no_payload_but_a_dlc() {
  auto frame = makeStandardRemoteFrame(0x7DF, 0);
  TEST_ASSERT_TRUE(frame.has_value());
  TEST_ASSERT_TRUE(frame->remoteRequest);
  TEST_ASSERT_EQUAL_UINT8(0, frame->dlc);
}

// --- Invalid frames never reach ICanPort::send() ----------------------

void test_invalid_frame_cannot_reach_send() {
  // makeStandardFrame returns nullopt for an out-of-range id; there is no
  // CanFrame value to construct, so nothing can be passed to send().
  auto invalid = makeStandardFrame(0x800, {0x01});
  TEST_ASSERT_FALSE(invalid.has_value());
}

// --- Filter matching truth table ---------------------------------------

void test_matches_filter_mask_and_value() {
  // Accept only 0x7E8..0x7EF: mask keeps the low 3 bits free.
  CanFilter filter{/*mask=*/0x7F8, /*filterValue=*/0x7E8};
  TEST_ASSERT_TRUE(matchesFilter(0x7E8, filter));
  TEST_ASSERT_TRUE(matchesFilter(0x7EF, filter));
  TEST_ASSERT_FALSE(matchesFilter(0x7E7, filter));
  TEST_ASSERT_FALSE(matchesFilter(0x7F0, filter));
}

void test_exact_id_filter() {
  CanFilter filter = exactIdFilter(0x7E8);
  TEST_ASSERT_TRUE(matchesFilter(0x7E8, filter));
  TEST_ASSERT_FALSE(matchesFilter(0x7E9, filter));
}

void test_default_obd_response_ranges() {
  TEST_ASSERT_TRUE(obd::isDefaultObdResponse11Bit(0x7E8));
  TEST_ASSERT_TRUE(obd::isDefaultObdResponse11Bit(0x7EF));
  TEST_ASSERT_FALSE(obd::isDefaultObdResponse11Bit(0x7E7));
  TEST_ASSERT_FALSE(obd::isDefaultObdResponse11Bit(0x7F0));

  TEST_ASSERT_TRUE(obd::isDefaultObdResponse29Bit(0x18DAF100));
  TEST_ASSERT_TRUE(obd::isDefaultObdResponse29Bit(0x18DAF1FF));
  TEST_ASSERT_FALSE(obd::isDefaultObdResponse29Bit(0x18DAF0FF));
  TEST_ASSERT_FALSE(obd::isDefaultObdResponse29Bit(0x18DAF200));
}

// --- FakeCanPort now implements the real ICanPort -----------------------

void test_fake_can_port_preserves_tx_order() {
  FakeCanPort port;
  port.send(CanFrameBuilder::standard(0x7E0).data({0x02, 0x01, 0x0C}), 100);
  port.send(CanFrameBuilder::standard(0x7E1).data({0x02, 0x01, 0x0D}), 100);

  TEST_ASSERT_EQUAL(2, port.transmitted().size());
  TEST_ASSERT_EQUAL_UINT32(0x7E0, port.transmitted()[0].id);
  TEST_ASSERT_EQUAL_UINT32(0x7E1, port.transmitted()[1].id);
}

void test_fake_can_port_rx_is_queued_and_non_blocking() {
  FakeCanPort port;
  port.queueRx(CanFrameBuilder::standard(0x7E8).data(
      {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}));

  ReceiveResult received = port.receive();
  TEST_ASSERT_TRUE(received.hasFrame);
  TEST_ASSERT_EQUAL_UINT32(0x7E8, received.frame.id);
  TEST_ASSERT_EQUAL_UINT8(8, received.frame.dlc);
  TEST_ASSERT_EQUAL_UINT8(0x06, received.frame.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0x41, received.frame.data[1]);

  ReceiveResult none = port.receive();
  TEST_ASSERT_FALSE(none.hasFrame);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_standard_id_boundary);
  RUN_TEST(test_extended_id_boundary);
  RUN_TEST(test_dlc_boundary);
  RUN_TEST(test_remote_frame_carries_no_payload_but_a_dlc);
  RUN_TEST(test_invalid_frame_cannot_reach_send);
  RUN_TEST(test_matches_filter_mask_and_value);
  RUN_TEST(test_exact_id_filter);
  RUN_TEST(test_default_obd_response_ranges);
  RUN_TEST(test_fake_can_port_preserves_tx_order);
  RUN_TEST(test_fake_can_port_rx_is_queued_and_non_blocking);
  return UNITY_END();
}
