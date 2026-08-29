#include <unity.h>

#include "../../support/fake_can_port.h"
#include "../../support/in_memory_settings_store.h"
#include "app/elm_application.h"

using namespace esp_obd;
using namespace esp_obd::can;

void setUp() {}
void tearDown() {}

void test_constructs_from_adapters_and_executes_at_commands() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);

  elm::ElmReply reply = application.execute(0, "ATI");
  TEST_ASSERT_EQUAL_STRING("ELM327 v2.2\r\r", reply.text.c_str());
  TEST_ASSERT_FALSE(application.diagnosticPending());
}

void test_exposes_the_injected_can_port() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);

  TEST_ASSERT_EQUAL_PTR(&port, &application.canPort());
}

void test_settings_commands_reach_the_injected_store() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);

  application.execute(0, "AT@3AABBCCDDEEFF");
  TEST_ASSERT_EQUAL_STRING("AABBCCDDEEFF", store.deviceId());
}

// --- Diagnostic request wiring: hex line -> DiagnosticTransport -> reply ---

void test_hex_request_auto_searches_and_completes_synchronously() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);

  // A Single Frame reply is available immediately, so the whole
  // auto-search + request/response cycle finishes within this one
  // execute() call (SP6, 11-bit/500k, is tried first).
  port.queueRx(*makeStandardFrame(0x7E8, {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}, 8));

  elm::ElmReply reply = application.execute(0, "0100");
  TEST_ASSERT_EQUAL(static_cast<int>(elm::ElmReplyKind::DiagnosticRequest),
                     static_cast<int>(reply.kind));
  TEST_ASSERT_FALSE(application.diagnosticPending());

  elm::ElmReply finalReply = application.takeDiagnosticReply();
  TEST_ASSERT_EQUAL_STRING("41 00 BE 1F B8 10\r\r", finalReply.text.c_str());
  TEST_ASSERT_TRUE(application.engine().session().protocolConnected);
}

void test_second_request_reuses_the_discovered_protocol_without_research() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);

  port.queueRx(*makeStandardFrame(0x7E8, {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}, 8));
  application.execute(0, "0100");
  TEST_ASSERT_TRUE(application.engine().session().protocolConnected);

  // No auto-search this time. Without ATSH, "010C" targets the functional
  // broadcast id and would otherwise wait out the full timeout collecting
  // up to 8 responses; the trailing "1" caps it at one response so this
  // completes as soon as the single queued frame is read.
  port.queueRx(*makeStandardFrame(0x7E8, {0x04, 0x41, 0x0C, 0x1A, 0xF8, 0, 0, 0}, 8));
  elm::ElmReply reply = application.execute(10, "010C1");
  TEST_ASSERT_FALSE(application.diagnosticPending());
  TEST_ASSERT_EQUAL_STRING("41 0C 1A F8\r\r", application.takeDiagnosticReply().text.c_str());
}

void test_diagnostic_pending_until_polled_to_completion() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);
  application.engine().session().protocol = elm::ElmProtocol::Iso15765_11bit_500k;
  application.engine().session().protocolConnected = true;

  // Trailing "1" caps this at one response, same reasoning as above.
  application.execute(0, "01001");
  TEST_ASSERT_TRUE(application.diagnosticPending());  // nothing queued yet: still waiting

  port.queueRx(*makeStandardFrame(0x7E8, {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}, 8));
  bool ready = application.poll(10);
  TEST_ASSERT_TRUE(ready);
  TEST_ASSERT_FALSE(application.diagnosticPending());
  TEST_ASSERT_EQUAL_STRING("41 00 BE 1F B8 10\r\r", application.takeDiagnosticReply().text.c_str());
}

void test_no_response_exhausts_auto_search_to_unable_to_connect() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);

  application.execute(0, "0100");
  TEST_ASSERT_TRUE(application.diagnosticPending());

  // 4 candidates x 200ms default timeout each, nothing ever answers.
  bool ready = false;
  for (Milliseconds t = 200; t <= 800 && !ready; t += 200) {
    ready = application.poll(t);
  }
  TEST_ASSERT_TRUE(ready);
  TEST_ASSERT_EQUAL_STRING("UNABLE TO CONNECT\r\r", application.takeDiagnosticReply().text.c_str());
}

// --- Headers-on padding trim (ATD0), confirmed missing against a real
// scanner-app capture of a 29-bit response: "18DAF1EF0641009018801155"
// (id 18DAF1EF, single frame PCI 06, payload 41 00 90 18 80 11, and a
// trailing 0x55 pad byte that ATD0 must not show). --------------------------

void test_headers_on_atd0_trims_padding_for_single_frame_response() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATSP7");  // fixed 29-bit/500k: avoids an auto-search delay
  app.execute(0, "ATH1");   // headers on; ATD0 is already the default

  port.queueRx(
      *makeExtendedFrame(0x18DAF1EF, {0x06, 0x41, 0x00, 0x90, 0x18, 0x80, 0x11, 0x55}, 8));
  app.execute(0, "2201101");  // trailing "1" caps at 1 response
  TEST_ASSERT_FALSE(app.diagnosticPending());

  // ATH1 shows raw frame bytes (the PCI included), just trimmed to drop
  // the trailing pad byte -- matching the contract's own worked example
  // "7E8 06 41 00 BE 1F B8 10" (PCI "06" present, pad byte dropped).
  TEST_ASSERT_EQUAL_STRING("18DAF1EF 06 41 00 90 18 80 11\r\r",
                            app.takeDiagnosticReply().text.c_str());
}

void test_headers_on_atd1_shows_full_raw_frame_including_padding() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATSP7");
  app.execute(0, "ATH1");
  app.execute(0, "ATD1");

  port.queueRx(
      *makeExtendedFrame(0x18DAF1EF, {0x06, 0x41, 0x00, 0x90, 0x18, 0x80, 0x11, 0x55}, 8));
  app.execute(0, "2201101");
  TEST_ASSERT_FALSE(app.diagnosticPending());

  TEST_ASSERT_EQUAL_STRING("18DAF1EF 8 06 41 00 90 18 80 11 55\r\r",
                            app.takeDiagnosticReply().text.c_str());
}

// Confirmed against a real scanner app: with ATH1 active, a multi-frame
// VIN response only showed its first raw frame (~3-6 characters of the
// 17-char VIN), because only rawFrames[0] was ever rendered. This
// reproduces that exact shape (17-char VIN, FF + 2 CF, no CAN-level
// padding since 3 + 17 = 20 = 6 + 7 + 7 divides evenly) and asserts every
// raw frame appears as its own line.
void test_headers_on_shows_every_raw_frame_for_a_multi_frame_vin_response() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATSH7E0");
  app.execute(0, "ATH1");

  // "49 02 01" + ASCII "1HGCM82633A004352" (17 chars) = 20-byte payload.
  port.queueRx(*makeStandardFrame(0x7E8, {0x10, 0x14, 0x49, 0x02, 0x01, 0x31, 0x48, 0x47}, 8));
  app.execute(0, "09021");  // trailing "1" caps at 1 response
  TEST_ASSERT_TRUE(app.diagnosticPending());  // still waiting on the CFs

  port.queueRx(*makeStandardFrame(0x7E8, {0x21, 0x43, 0x4D, 0x38, 0x32, 0x36, 0x33, 0x33}, 8));
  TEST_ASSERT_FALSE(app.poll(10));
  port.queueRx(*makeStandardFrame(0x7E8, {0x22, 0x41, 0x30, 0x30, 0x34, 0x33, 0x35, 0x32}, 8));
  TEST_ASSERT_TRUE(app.poll(20));

  TEST_ASSERT_EQUAL_STRING(
      "7E8 10 14 49 02 01 31 48 47\r\r"
      "7E8 21 43 4D 38 32 36 33 33\r\r"
      "7E8 22 41 30 30 34 33 35 32\r\r",
      app.takeDiagnosticReply().text.c_str());
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_constructs_from_adapters_and_executes_at_commands);
  RUN_TEST(test_exposes_the_injected_can_port);
  RUN_TEST(test_settings_commands_reach_the_injected_store);
  RUN_TEST(test_hex_request_auto_searches_and_completes_synchronously);
  RUN_TEST(test_second_request_reuses_the_discovered_protocol_without_research);
  RUN_TEST(test_diagnostic_pending_until_polled_to_completion);
  RUN_TEST(test_no_response_exhausts_auto_search_to_unable_to_connect);
  RUN_TEST(test_headers_on_atd0_trims_padding_for_single_frame_response);
  RUN_TEST(test_headers_on_atd1_shows_full_raw_frame_including_padding);
  RUN_TEST(test_headers_on_shows_every_raw_frame_for_a_multi_frame_vin_response);
  return UNITY_END();
}
