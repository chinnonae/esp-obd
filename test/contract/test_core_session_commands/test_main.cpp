#include <unity.h>

#include <string>

#include "../../support/in_memory_settings_store.h"
#include "core/build_info.h"
#include "elm/elm_command_engine.h"

using namespace esp_obd::elm;

void setUp() {}
void tearDown() {}

// --- ATZ / ATWS / ATD: reset and every default in one test ---------------
// docs/tasks/INDEX.md's contract, and ELM_COMMAND_BEHAVIOR.md section 4
// item 1: "Run ATZ, then assert every default in one test; do not rely on
// test order."

static void assertSessionIsDefault(const ElmSession& s) {
  TEST_ASSERT_TRUE(s.echoEnabled);
  TEST_ASSERT_FALSE(s.linefeedsEnabled);
  TEST_ASSERT_TRUE(s.spacesEnabled);
  TEST_ASSERT_FALSE(s.headersEnabled);
  TEST_ASSERT_FALSE(s.displayDlcEnabled);
  TEST_ASSERT_TRUE(s.responsesEnabled);
  TEST_ASSERT_TRUE(s.automaticFormattingEnabled);
  TEST_ASSERT_TRUE(s.automaticFlowControlEnabled);
  TEST_ASSERT_FALSE(s.allowLongMessagesEnabled);
  TEST_ASSERT_EQUAL(static_cast<int>(AdaptiveTiming::Mode1), static_cast<int>(s.adaptiveTiming));
  TEST_ASSERT_EQUAL(static_cast<int>(ElmProtocol::AutomaticSearch), static_cast<int>(s.protocol));
  TEST_ASSERT_FALSE(s.protocolConnected);
  TEST_ASSERT_EQUAL_UINT32(0x7DF, s.requestId);
  TEST_ASSERT_FALSE(s.receiveAddress.has_value());
  TEST_ASSERT_FALSE(s.idFilter.has_value());
  TEST_ASSERT_EQUAL_UINT32(200, s.responseTimeoutMs);
  TEST_ASSERT_FALSE(s.customHeaderId.has_value());
  TEST_ASSERT_FALSE(s.extendedAddressingEnabled);
  TEST_ASSERT_FALSE(s.monitorActive);

  // T09 additions.
  TEST_ASSERT_FALSE(s.protocolDiscoveredViaAutoSearch);
  TEST_ASSERT_EQUAL_UINT8(1, s.canTimeoutMultiplier);
  TEST_ASSERT_EQUAL_UINT8(0x18, s.priorityBits);
  TEST_ASSERT_EQUAL(static_cast<int>(FlowControlMode::Automatic), static_cast<int>(s.flowControlMode));
  TEST_ASSERT_FALSE(s.manualFlowControlId.has_value());
  TEST_ASSERT_EQUAL_UINT8(0, s.manualFlowControlDataLen);
  TEST_ASSERT_FALSE(s.requiredExtendedAddressByte.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(MonitorMode::None), static_cast<int>(s.monitorMode));
  TEST_ASSERT_EQUAL_UINT8(0, s.monitorAddressByte);
  TEST_ASSERT_FALSE(s.silentMonitoringEnabled);
  TEST_ASSERT_FALSE(s.variableDlcEnabled);
  TEST_ASSERT_FALSE(s.lastAcceptedReceivedFrame.has_value());
}

void test_atz_resets_every_default_and_returns_identity() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  ElmReply reply = engine.execute("ATZ");
  TEST_ASSERT_TRUE(std::string(reply.text.c_str()).rfind(esp_obd::build::kElmVersion, 0) == 0);
  assertSessionIsDefault(engine.session());
}

void test_atws_behaves_like_atz() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  // Mutate away from defaults first, to prove ATWS actually resets.
  engine.execute("ATE0");
  engine.execute("ATH1");

  ElmReply reply = engine.execute("ATWS");
  TEST_ASSERT_TRUE(std::string(reply.text.c_str()).rfind(esp_obd::build::kElmVersion, 0) == 0);
  assertSessionIsDefault(engine.session());
}

void test_atd_resets_defaults_and_returns_ok() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  engine.execute("ATE0");

  ElmReply reply = engine.execute("ATD");
  TEST_ASSERT_TRUE(std::string(reply.text.c_str()).rfind("OK", 0) == 0);
  assertSessionIsDefault(engine.session());
}

void test_ati_returns_identity_without_state_change() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  engine.execute("ATE0");
  ElmSession before = engine.session();

  ElmReply reply = engine.execute("ATI");
  TEST_ASSERT_TRUE(std::string(reply.text.c_str()).rfind(esp_obd::build::kElmVersion, 0) == 0);
  TEST_ASSERT_TRUE(before == engine.session());
}

void test_at1_returns_adapter_description() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  ElmReply reply = engine.execute("AT@1");
  TEST_ASSERT_TRUE(std::string(reply.text.c_str()).rfind(esp_obd::build::kAdapterDescription, 0) ==
                    0);
}

// --- Toggles: ATE, ATL, ATS, ATH, ATD0/1, ATR -----------------------------

void test_ate_toggles_echo() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  engine.execute("ATE0");
  TEST_ASSERT_FALSE(engine.session().echoEnabled);
  engine.execute("ATE1");
  TEST_ASSERT_TRUE(engine.session().echoEnabled);
}

void test_atl_ok_uses_new_line_ending_mode() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  ElmReply reply = engine.execute("ATL1");
  // The OK for *this* command already uses the new (just-set) L1 ending.
  TEST_ASSERT_EQUAL_STRING("OK\r\n\r\n", reply.text.c_str());
  TEST_ASSERT_TRUE(engine.session().linefeedsEnabled);
}

void test_ats_toggles_spaces() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  engine.execute("ATS0");
  TEST_ASSERT_FALSE(engine.session().spacesEnabled);
  engine.execute("ATS1");
  TEST_ASSERT_TRUE(engine.session().spacesEnabled);
}

void test_ath_toggles_headers() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  engine.execute("ATH1");
  TEST_ASSERT_TRUE(engine.session().headersEnabled);
  engine.execute("ATH0");
  TEST_ASSERT_FALSE(engine.session().headersEnabled);
}

void test_atd01_toggles_display_dlc_only() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  engine.execute("ATD1");
  TEST_ASSERT_TRUE(engine.session().displayDlcEnabled);
  engine.execute("ATD0");
  TEST_ASSERT_FALSE(engine.session().displayDlcEnabled);
}

void test_atr_toggles_responses() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  engine.execute("ATR0");
  TEST_ASSERT_FALSE(engine.session().responsesEnabled);
  engine.execute("ATR1");
  TEST_ASSERT_TRUE(engine.session().responsesEnabled);
}

// --- Empty-command repeat and malformed-command isolation -----------------

void test_empty_command_before_any_previous_produces_no_response() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  ElmReply reply = engine.execute("");
  TEST_ASSERT_EQUAL(static_cast<int>(ElmReplyKind::NoReply), static_cast<int>(reply.kind));
  TEST_ASSERT_FALSE(reply.appendPrompt);
}

void test_empty_command_repeats_last_recognized_command() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  engine.execute("ATE0");
  ElmReply repeated = engine.execute("");
  TEST_ASSERT_EQUAL_STRING("OK\r\r", repeated.text.c_str());
  TEST_ASSERT_FALSE(engine.session().echoEnabled);  // ATE0's effect, run again
}

void test_malformed_command_does_not_become_the_repeat_target() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  engine.execute("ATBOGUS");        // malformed: returns '?', not stored
  ElmReply empty = engine.execute("");  // still "no previous [recognized] command"
  TEST_ASSERT_EQUAL(static_cast<int>(ElmReplyKind::NoReply), static_cast<int>(empty.kind));
}

void test_malformed_command_leaves_session_unchanged() {
  InMemorySettingsStore engineStore;
  ElmCommandEngine engine(engineStore);
  engine.execute("ATE0");
  ElmSession before = engine.session();

  ElmReply reply = engine.execute("ATBOGUS");
  TEST_ASSERT_EQUAL_STRING("?\r\r", reply.text.c_str());
  TEST_ASSERT_TRUE(before == engine.session());
}

void test_command_is_case_insensitive_and_ignores_spaces() {
  InMemorySettingsStore engineStore1;
  ElmCommandEngine engine1(engineStore1);
  InMemorySettingsStore engineStore2;
  ElmCommandEngine engine2(engineStore2);
  ElmReply r1 = engine1.execute("ate0");
  ElmReply r2 = engine2.execute("AT E0");
  TEST_ASSERT_EQUAL_STRING(r1.text.c_str(), r2.text.c_str());
  TEST_ASSERT_FALSE(engine1.session().echoEnabled);
  TEST_ASSERT_FALSE(engine2.session().echoEnabled);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_atz_resets_every_default_and_returns_identity);
  RUN_TEST(test_atws_behaves_like_atz);
  RUN_TEST(test_atd_resets_defaults_and_returns_ok);
  RUN_TEST(test_ati_returns_identity_without_state_change);
  RUN_TEST(test_at1_returns_adapter_description);
  RUN_TEST(test_ate_toggles_echo);
  RUN_TEST(test_atl_ok_uses_new_line_ending_mode);
  RUN_TEST(test_ats_toggles_spaces);
  RUN_TEST(test_ath_toggles_headers);
  RUN_TEST(test_atd01_toggles_display_dlc_only);
  RUN_TEST(test_atr_toggles_responses);
  RUN_TEST(test_empty_command_before_any_previous_produces_no_response);
  RUN_TEST(test_empty_command_repeats_last_recognized_command);
  RUN_TEST(test_malformed_command_does_not_become_the_repeat_target);
  RUN_TEST(test_malformed_command_leaves_session_unchanged);
  RUN_TEST(test_command_is_case_insensitive_and_ignores_spaces);
  return UNITY_END();
}
