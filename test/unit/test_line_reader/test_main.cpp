#include <unity.h>

#include "app/line_reader.h"

using namespace esp_obd::app;

void setUp() {}
void tearDown() {}

namespace {
void feed(LineReader& reader, const char* text) {
  for (const char* p = text; *p != '\0'; ++p) {
    LineEvent event = reader.onByte(0, static_cast<uint8_t>(*p));
    TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::None), static_cast<int>(event.kind));
  }
}
}  // namespace

void test_line_completes_on_cr() {
  LineReader reader(200);
  feed(reader, "ATZ");
  LineEvent event = reader.onByte(0, '\r');
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::Line), static_cast<int>(event.kind));
  TEST_ASSERT_EQUAL_STRING("ATZ", event.text.c_str());
}

void test_following_linefeed_after_cr_is_swallowed() {
  LineReader reader(200);
  feed(reader, "ATZ");
  reader.onByte(0, '\r');
  LineEvent event = reader.onByte(0, '\n');
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::None), static_cast<int>(event.kind));
}

void test_linefeed_not_following_cr_is_a_normal_byte() {
  LineReader reader(200);
  // An LF with no preceding CR is just an ordinary (odd) byte, not swallowed.
  LineEvent event = reader.onByte(0, '\n');
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::None), static_cast<int>(event.kind));
  LineEvent lineEvent = reader.onByte(0, '\r');
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::Line), static_cast<int>(lineEvent.kind));
  TEST_ASSERT_EQUAL(1, lineEvent.text.size());
}

void test_empty_line_is_a_valid_line_event() {
  LineReader reader(200);
  LineEvent event = reader.onByte(0, '\r');
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::Line), static_cast<int>(event.kind));
  TEST_ASSERT_TRUE(event.text.empty());
}

void test_overflow_discards_buffer_and_resumes_fresh() {
  LineReader reader(200);
  LineEvent overflowEvent;
  for (size_t i = 0; i < kLineReaderCapacity + 1; ++i) {
    overflowEvent = reader.onByte(0, 'A');
  }
  // The (capacity+1)-th byte tips it over: the check runs before the
  // append, so filling exactly to capacity doesn't overflow by itself.
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::Overflow), static_cast<int>(overflowEvent.kind));

  // The reader resumed cleanly: a fresh short line still completes normally.
  feed(reader, "ATZ");
  LineEvent lineEvent = reader.onByte(0, '\r');
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::Line), static_cast<int>(lineEvent.kind));
  TEST_ASSERT_EQUAL_STRING("ATZ", lineEvent.text.c_str());
}

void test_poll_times_out_a_partial_line() {
  LineReader reader(100);
  feed(reader, "AT");

  LineEvent notYet = reader.poll(99);
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::None), static_cast<int>(notYet.kind));

  LineEvent timedOut = reader.poll(100);
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::TimedOut), static_cast<int>(timedOut.kind));
}

void test_poll_never_fires_with_nothing_pending() {
  LineReader reader(100);
  LineEvent event = reader.poll(1000000);
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::None), static_cast<int>(event.kind));
}

void test_poll_does_not_refire_after_a_timeout_already_reset_it() {
  LineReader reader(100);
  feed(reader, "AT");
  reader.poll(100);  // fires TimedOut, clears pending
  LineEvent event = reader.poll(1000);
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::None), static_cast<int>(event.kind));
}

void test_disconnect_discards_the_buffer() {
  LineReader reader(200);
  feed(reader, "AT");
  LineEvent event = reader.notifyDisconnected();
  TEST_ASSERT_EQUAL(static_cast<int>(LineEventKind::Disconnected), static_cast<int>(event.kind));

  feed(reader, "ATZ");
  LineEvent lineEvent = reader.onByte(0, '\r');
  TEST_ASSERT_EQUAL_STRING("ATZ", lineEvent.text.c_str());  // not "ATATZ"
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_line_completes_on_cr);
  RUN_TEST(test_following_linefeed_after_cr_is_swallowed);
  RUN_TEST(test_linefeed_not_following_cr_is_a_normal_byte);
  RUN_TEST(test_empty_line_is_a_valid_line_event);
  RUN_TEST(test_overflow_discards_buffer_and_resumes_fresh);
  RUN_TEST(test_poll_times_out_a_partial_line);
  RUN_TEST(test_poll_never_fires_with_nothing_pending);
  RUN_TEST(test_poll_does_not_refire_after_a_timeout_already_reset_it);
  RUN_TEST(test_disconnect_discards_the_buffer);
  return UNITY_END();
}
