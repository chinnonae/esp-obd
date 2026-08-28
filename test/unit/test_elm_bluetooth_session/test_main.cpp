#include <unity.h>

#include <string>

#include "../../support/fake_can_port.h"
#include "../../support/in_memory_settings_store.h"
#include "app/elm_bluetooth_session.h"

using namespace esp_obd;
using namespace esp_obd::can;

void setUp() {}
void tearDown() {}

namespace {
// Feeds a line (including its trailing '\r') and returns everything the
// session wrote back, concatenated -- a captured Bluetooth conversation.
app::TransportOutput sendLine(app::ElmBluetoothSession& session, Milliseconds now,
                               const char* line) {
  app::TransportOutput out;
  for (const char* p = line; *p != '\0'; ++p) {
    out += session.onByte(now, static_cast<uint8_t>(*p)).c_str();
  }
  return out;
}
}  // namespace

void test_atz_conversation_echoes_then_replies_with_prompt() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);

  app::TransportOutput out = sendLine(session, 0, "ATZ\r");
  TEST_ASSERT_EQUAL_STRING("ATZ\rELM327 v2.2\r\r>", out.c_str());
}

void test_ate0_conversation_then_next_command_is_not_echoed() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);

  app::TransportOutput first = sendLine(session, 0, "ATE0\r");
  // "ATE0" itself is echoed (echo was still on when these bytes arrived);
  // only *later* commands see the new setting.
  TEST_ASSERT_EQUAL_STRING("ATE0\rOK\r\r>", first.c_str());

  app::TransportOutput second = sendLine(session, 10, "ATI\r");
  TEST_ASSERT_EQUAL_STRING("ELM327 v2.2\r\r>", second.c_str());  // no echo this time
}

void test_0100_conversation_captures_the_full_diagnostic_reply() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);

  port.queueRx(*makeStandardFrame(0x7E8, {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}, 8));

  app::TransportOutput out = sendLine(session, 0, "0100\r");
  TEST_ASSERT_EQUAL_STRING("0100\r41 00 BE 1F B8 10\r\r>", out.c_str());
}

void test_diagnostic_reply_arrives_later_via_poll_not_onbyte() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);
  application.engine().session().protocol = elm::ElmProtocol::Iso15765_11bit_500k;
  application.engine().session().protocolConnected = true;

  app::TransportOutput sent = sendLine(session, 0, "01001\r");  // capped at 1 response
  TEST_ASSERT_EQUAL_STRING("01001\r", sent.c_str());  // nothing queued yet: no reply

  port.queueRx(*makeStandardFrame(0x7E8, {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}, 8));
  app::TransportOutput polled = session.poll(10);
  TEST_ASSERT_EQUAL_STRING("41 00 BE 1F B8 10\r\r>", polled.c_str());
}

// --- Monitor stop -------------------------------------------------------

void test_any_byte_stops_monitor_mode_and_is_discarded() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);
  application.engine().session().monitorActive = true;

  app::TransportOutput out = session.onByte(0, 'X');
  TEST_ASSERT_EQUAL_STRING("STOPPED\r\r>", out.c_str());
  TEST_ASSERT_FALSE(application.engine().session().monitorActive);

  // The stopping byte itself was discarded, not treated as the start of a
  // new command: a fresh line still assembles correctly afterward.
  app::TransportOutput next = sendLine(session, 10, "ATI\r");
  TEST_ASSERT_EQUAL_STRING("ATI\rELM327 v2.2\r\r>", next.c_str());
}

// --- Overflow -------------------------------------------------------------

void test_overflow_responds_with_unknown_command() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);

  app::TransportOutput out;
  for (size_t i = 0; i < app::kLineReaderCapacity + 1; ++i) {
    out = session.onByte(0, 'A');
  }
  // Every 'A' up to the overflow byte was echoed, then "?" + ending + prompt.
  TEST_ASSERT_TRUE(out.size() > 0);
  std::string text(out.c_str());
  TEST_ASSERT_TRUE(text.find("?\r\r>") != std::string::npos);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_atz_conversation_echoes_then_replies_with_prompt);
  RUN_TEST(test_ate0_conversation_then_next_command_is_not_echoed);
  RUN_TEST(test_0100_conversation_captures_the_full_diagnostic_reply);
  RUN_TEST(test_diagnostic_reply_arrives_later_via_poll_not_onbyte);
  RUN_TEST(test_any_byte_stops_monitor_mode_and_is_discarded);
  RUN_TEST(test_overflow_responds_with_unknown_command);
  return UNITY_END();
}
