#include <unity.h>

#include "../../support/elm_reply_assertions.h"
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

void test_expect_reply_helper_compares_strings() {
  expectReply("41 00 BE 1F B8 10\r\r").toEqual("41 00 BE 1F B8 10\r\r");
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_runner_executes);
  RUN_TEST(test_fake_clock_advances_only_when_instructed);
  RUN_TEST(test_expect_reply_helper_compares_strings);
  return UNITY_END();
}
