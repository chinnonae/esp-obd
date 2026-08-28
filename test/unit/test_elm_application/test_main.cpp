#include <unity.h>

#include "../../support/fake_can_port.h"
#include "../../support/in_memory_settings_store.h"
#include "app/elm_application.h"

using namespace esp_obd;

void setUp() {}
void tearDown() {}

void test_constructs_from_adapters_and_executes_at_commands() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication application(port, store);

  elm::ElmReply reply = application.execute("ATI");
  TEST_ASSERT_EQUAL_STRING("ELM327 v2.2\r\r", reply.text.c_str());
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

  application.execute("AT@3AABBCCDDEEFF");
  TEST_ASSERT_EQUAL_STRING("AABBCCDDEEFF", store.deviceId());
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_constructs_from_adapters_and_executes_at_commands);
  RUN_TEST(test_exposes_the_injected_can_port);
  RUN_TEST(test_settings_commands_reach_the_injected_store);
  return UNITY_END();
}
