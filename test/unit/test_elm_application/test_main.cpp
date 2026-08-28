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

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_constructs_from_adapters_and_executes_at_commands);
  RUN_TEST(test_exposes_the_injected_can_port);
  RUN_TEST(test_settings_commands_reach_the_injected_store);
  RUN_TEST(test_hex_request_auto_searches_and_completes_synchronously);
  RUN_TEST(test_second_request_reuses_the_discovered_protocol_without_research);
  RUN_TEST(test_diagnostic_pending_until_polled_to_completion);
  RUN_TEST(test_no_response_exhausts_auto_search_to_unable_to_connect);
  return UNITY_END();
}
