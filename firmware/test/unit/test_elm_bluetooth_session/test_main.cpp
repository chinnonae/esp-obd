#include <unity.h>

#include <array>
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
  TEST_ASSERT_EQUAL_STRING("ATZ\rELM327 v1.5\r\r>", out.c_str());
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
  TEST_ASSERT_EQUAL_STRING("ELM327 v1.5\r\r>", second.c_str());  // no echo this time
}

void test_0100_conversation_captures_the_full_diagnostic_reply() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);

  port.queueRx(*makeStandardFrame(0x7E8, {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}, 8));

  // First request on SP0: the auto-search announces itself, as an ELM327 does.
  app::TransportOutput out = sendLine(session, 0, "01001\r");
  TEST_ASSERT_EQUAL_STRING("01001\rSEARCHING...\r41 00 BE 1F B8 10\r\r>", out.c_str());

  // Once connected, no SEARCHING... again.
  port.queueRx(*makeStandardFrame(0x7E8, {0x04, 0x41, 0x0C, 0x1A, 0xF8, 0, 0, 0}, 8));
  TEST_ASSERT_EQUAL_STRING("010C1\r41 0C 1A F8\r\r>", sendLine(session, 10, "010C1\r").c_str());
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
  TEST_ASSERT_EQUAL_STRING("ATI\rELM327 v1.5\r\r>", next.c_str());
}

// --- Interrupting a running request (ELM327 "STOPPED") -----------------------

void test_byte_during_a_pending_request_stops_it_and_is_discarded() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);
  application.engine().session().protocol = elm::ElmProtocol::Iso15765_11bit_500k;
  application.engine().session().protocolConnected = true;

  sendLine(session, 0, "0100\r");  // functional: still collecting
  TEST_ASSERT_TRUE(application.diagnosticPending());

  // As in the reference adapter's Car Scanner log: "ATRV" sent mid-request
  // gets only STOPPED; the ATRV line itself is not executed.
  app::TransportOutput out = sendLine(session, 10, "ATRV\r");
  TEST_ASSERT_EQUAL_STRING("STOPPED\r\r>", out.c_str());
  TEST_ASSERT_FALSE(application.diagnosticPending());

  // A reply arriving afterwards is not printed; the next line works normally.
  port.queueRx(*makeStandardFrame(0x7E8, {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}, 8));
  TEST_ASSERT_EQUAL_STRING("", session.poll(20).c_str());
  TEST_ASSERT_EQUAL_STRING("ATI\rELM327 v1.5\r\r>", sendLine(session, 30, "ATI\r").c_str());
}

void test_a_bare_cr_interrupt_is_consumed_whole() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);
  application.engine().session().protocol = elm::ElmProtocol::Iso15765_11bit_500k;
  application.engine().session().protocolConnected = true;

  sendLine(session, 0, "0100\r");
  TEST_ASSERT_EQUAL_STRING("STOPPED\r\r>", session.onByte(10, '\r').c_str());
  TEST_ASSERT_EQUAL_STRING("ATI\rELM327 v1.5\r\r>", sendLine(session, 20, "ATI\r").c_str());
}

void test_byte_after_the_reply_arrived_but_before_poll_gets_the_reply_first() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);
  application.engine().session().protocol = elm::ElmProtocol::Iso15765_11bit_500k;
  application.engine().session().protocolConnected = true;

  sendLine(session, 0, "01001\r");
  port.queueRx(*makeStandardFrame(0x7E8, {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}, 8));

  // The reply is already on the bus: print it, then echo 'A' as normal input.
  app::TransportOutput out = session.onByte(10, 'A');
  TEST_ASSERT_EQUAL_STRING("41 00 BE 1F B8 10\r\r>A", out.c_str());
}

// --- ATZ discards input already received ---------------------------------------

// WiCAN documents Car Scanner sending "ATZ\rATE0\r" in one write. A real
// ELM327 loses the ATE0 while resetting; answering both desyncs the app.
void test_input_received_with_atz_is_discarded() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);

  app::TransportOutput out = sendLine(session, 0, "ATZ\rATE0\r");
  TEST_ASSERT_EQUAL_STRING("ATZ\rELM327 v1.5\r\r>", out.c_str());
  session.poll(0);

  // Echo is still on, proving ATE0 was never executed.
  TEST_ASSERT_EQUAL_STRING("ATI\rELM327 v1.5\r\r>", sendLine(session, 10, "ATI\r").c_str());
}

// Car Scanner then sends bare CRs while waiting for the reset banner. The
// reference adapter stays silent; old ESP-OBD firmware answered "OK>" ten
// times (repeating ATE0), and repeating ATZ would reset over and over.
void test_bare_crs_after_atz_produce_no_reply() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);

  sendLine(session, 0, "ATE0\r");
  sendLine(session, 1, "ATZ\r");
  session.poll(1);
  app::TransportOutput out;
  for (int i = 0; i < 10; ++i) {
    out += session.onByte(2 + i, '\r').c_str();
    session.poll(2 + i);
  }
  TEST_ASSERT_EQUAL_STRING("\r\r\r\r\r\r\r\r\r\r", out.c_str());  // echo only (ATZ turned echo back on)
}

void test_atal_is_accepted() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);
  TEST_ASSERT_EQUAL_STRING("ATAL\rOK\r\r>", sendLine(session, 0, "ATAL\r").c_str());
  TEST_ASSERT_TRUE(application.engine().session().allowLongMessagesEnabled);
  TEST_ASSERT_EQUAL_STRING("ATNL\rOK\r\r>", sendLine(session, 1, "ATNL\r").c_str());
  TEST_ASSERT_FALSE(application.engine().session().allowLongMessagesEnabled);
}

// --- Car Scanner init replay ----------------------------------------------

// Car Scanner 2.1.x ("Honda/Acura Hybrids" profile) connecting to the
// reference OBDII adapter, from the field log: the exact command sequence,
// and the six 29-bit ECU replies to the first 0100. Every reply below is
// what the reference adapter answered, except where noted.
void test_car_scanner_init_sequence_matches_reference_adapter() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  app::ElmBluetoothSession session(application);
  std::string out;
  Milliseconds now = 0;
  auto line = [&](const char* text) {
    out.clear();
    out += sendLine(session, now, text).c_str();
    out += session.poll(now).c_str();
    now += 5;
    return out;
  };

  // One write, then bare CRs while waiting for the banner: ATE0 is lost
  // in the reset and the CRs get no reply (only their echo: echo is on).
  TEST_ASSERT_EQUAL_STRING("ATZ\rELM327 v1.5\r\r>", line("ATZ\rATE0\r").c_str());
  for (int i = 0; i < 10; ++i) {
    TEST_ASSERT_EQUAL_STRING("\r", line("\r").c_str());
  }
  TEST_ASSERT_EQUAL_STRING("ATE0\rOK\r\r>", line("ATE0\r").c_str());
  TEST_ASSERT_EQUAL_STRING("OK\r\r>", line("ATE0\r").c_str());
  TEST_ASSERT_EQUAL_STRING("?\r\r>", line("STI\r").c_str());
  TEST_ASSERT_EQUAL_STRING("?\r\r>", line("VTI\r").c_str());
  // ATD restores defaults, so echo is back on for the next two commands:
  // the log shows exactly "ATDOK>ATD0ATD0OK>ATE0ATE0OK>".
  TEST_ASSERT_EQUAL_STRING("OK\r\r>", line("ATD\r").c_str());
  TEST_ASSERT_EQUAL_STRING("ATD0\rOK\r\r>", line("ATD0\r").c_str());
  TEST_ASSERT_EQUAL_STRING("ATE0\rOK\r\r>", line("ATE0\r").c_str());
  for (const char* cmd : {"ATH1\r", "ATSP0\r", "ATE0\r", "ATH1\r",
                          "ATM0\r", "ATS0\r", "ATAT1\r", "ATAL\r", "ATST64\r"}) {
    TEST_ASSERT_EQUAL_STRING_MESSAGE("OK\r\r>", line(cmd).c_str(), cmd);
  }

  // 0100 on SP0: SEARCHING..., 11-bit/500k stays silent for ATST64 (400ms),
  // then 29-bit/500k gets all six ECUs.
  TEST_ASSERT_EQUAL_STRING("SEARCHING...\r", line("0100\r").c_str());
  now += 400;
  TEST_ASSERT_EQUAL_STRING("", session.poll(now).c_str());  // moves on to SP7
  TEST_ASSERT_EQUAL_UINT32(0x18DB33F1, port.transmitted().back().id);
  const uint32_t ids[] = {0x18DAF1EF, 0x18DAF107, 0x18DAF110, 0x18DAF101, 0x18DAF102, 0x18DAF106};
  const std::array<uint8_t, 8> data[] = {
      {0x06, 0x41, 0x00, 0x90, 0x18, 0x80, 0x11, 0x55}, {0x06, 0x41, 0x00, 0x90, 0x18, 0x80, 0x13, 0x55},
      {0x06, 0x41, 0x00, 0xB6, 0x3C, 0xA8, 0x13, 0x55}, {0x06, 0x41, 0x00, 0x98, 0x18, 0x80, 0x13, 0x55},
      {0x06, 0x41, 0x00, 0x98, 0x18, 0x80, 0x13, 0x55}, {0x06, 0x41, 0x00, 0x98, 0x18, 0x80, 0x13, 0x55}};
  std::string reply;
  for (size_t i = 0; i < 6; ++i) {
    port.queueRx(*makeExtendedFrame(ids[i], data[i], 8));
    reply += session.poll(++now).c_str();
  }
  for (int i = 0; i < 1000 && application.diagnosticPending(); ++i) {
    reply += session.poll(++now).c_str();
  }
  TEST_ASSERT_EQUAL_STRING(
      "18DAF1EF06410090188011\r18DAF10706410090188013\r18DAF110064100B63CA813\r"
      "18DAF10106410098188013\r18DAF10206410098188013\r18DAF10606410098188013\r\r>",
      reply.c_str());

  TEST_ASSERT_EQUAL_STRING("A7\r\r>", line("ATDPN\r").c_str());
  // The reference adapter answers 12.xV here; this board has no voltage input.
  TEST_ASSERT_EQUAL_STRING("?\r\r>", line("ATRV\r").c_str());
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
  RUN_TEST(test_byte_during_a_pending_request_stops_it_and_is_discarded);
  RUN_TEST(test_byte_after_the_reply_arrived_but_before_poll_gets_the_reply_first);
  RUN_TEST(test_input_received_with_atz_is_discarded);
  RUN_TEST(test_a_bare_cr_interrupt_is_consumed_whole);
  RUN_TEST(test_bare_crs_after_atz_produce_no_reply);
  RUN_TEST(test_atal_is_accepted);
  RUN_TEST(test_car_scanner_init_sequence_matches_reference_adapter);
  return UNITY_END();
}
