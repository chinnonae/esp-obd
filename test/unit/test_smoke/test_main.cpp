#include <unity.h>

#include "../../support/can_frame_builder.h"
#include "../../support/elm_reply_assertions.h"
#include "../../support/fake_can_port.h"
#include "../../support/fake_clock.h"

void setUp() {}
void tearDown() {}

void test_runner_executes() { TEST_ASSERT_TRUE(true); }

void test_fake_clock_advances_only_when_instructed() {
  FakeClock clock;
  TEST_ASSERT_EQUAL_UINT32(0, clock.now());

  clock.advance(150);
  TEST_ASSERT_EQUAL_UINT32(150, clock.now());

  clock.advance(50);
  TEST_ASSERT_EQUAL_UINT32(200, clock.now());
}

void test_fake_can_port_preserves_tx_order() {
  FakeCanPort port;
  port.send(CanFrameBuilder::standard(0x7E0).data({0x02, 0x01, 0x0C}));
  port.send(CanFrameBuilder::standard(0x7E1).data({0x02, 0x01, 0x0D}));

  TEST_ASSERT_EQUAL(2, port.transmitted().size());
  TEST_ASSERT_EQUAL_UINT32(0x7E0, port.transmitted()[0].id);
  TEST_ASSERT_EQUAL_UINT32(0x7E1, port.transmitted()[1].id);
}

void test_fake_can_port_rx_is_queued_and_non_blocking() {
  FakeCanPort port;
  port.queueRx(CanFrameBuilder::standard(0x7E8).data(
      {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}));

  FakeCanFrame received;
  TEST_ASSERT_TRUE(port.receive(received));
  TEST_ASSERT_EQUAL_UINT32(0x7E8, received.id);
  TEST_ASSERT_EQUAL_UINT8(8, received.dlc);       // 8 bytes on the wire
  TEST_ASSERT_EQUAL_UINT8(0x06, received.data[0]); // ISO-TP PCI: single frame, length 6
  TEST_ASSERT_EQUAL_UINT8(0x41, received.data[1]);

  FakeCanFrame none;
  TEST_ASSERT_FALSE(port.receive(none));
}

void test_expect_reply_helper_compares_strings() {
  expectReply("41 00 BE 1F B8 10\r\r").toEqual("41 00 BE 1F B8 10\r\r");
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_runner_executes);
  RUN_TEST(test_fake_clock_advances_only_when_instructed);
  RUN_TEST(test_fake_can_port_preserves_tx_order);
  RUN_TEST(test_fake_can_port_rx_is_queued_and_non_blocking);
  RUN_TEST(test_expect_reply_helper_compares_strings);
  return UNITY_END();
}
