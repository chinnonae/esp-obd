#include <unity.h>

#include "../../support/in_memory_settings_store.h"
#include "elm/elm_command_engine.h"

using namespace esp_obd::elm;

void setUp() {}
void tearDown() {}

// --- AT@2 / AT@3 ------------------------------------------------------------

void test_at2_returns_default_device_id() {
  InMemorySettingsStore store;
  ElmCommandEngine engine(store);

  ElmReply reply = engine.execute("AT@2");
  TEST_ASSERT_EQUAL_STRING("FFFFFFFFFFFF\r\r", reply.text.c_str());
}

void test_at3_valid_twelve_hex_digits_is_stored_and_read_back() {
  InMemorySettingsStore store;
  ElmCommandEngine engine(store);

  ElmReply setReply = engine.execute("AT@3AABBCCDDEEFF");
  TEST_ASSERT_EQUAL_STRING("OK\r\r", setReply.text.c_str());

  ElmReply readReply = engine.execute("AT@2");
  TEST_ASSERT_EQUAL_STRING("AABBCCDDEEFF\r\r", readReply.text.c_str());
}

void test_at3_wrong_length_is_rejected_and_id_unchanged() {
  InMemorySettingsStore store;
  ElmCommandEngine engine(store);

  ElmReply setReply = engine.execute("AT@3AABBCC");  // 6 digits, not 12
  TEST_ASSERT_EQUAL_STRING("?\r\r", setReply.text.c_str());

  ElmReply readReply = engine.execute("AT@2");
  TEST_ASSERT_EQUAL_STRING("FFFFFFFFFFFF\r\r", readReply.text.c_str());
}

void test_at3_invalid_hex_is_rejected_and_id_unchanged() {
  InMemorySettingsStore store;
  ElmCommandEngine engine(store);

  ElmReply setReply = engine.execute("AT@3AABBCCDDEEGG");  // 'GG' not hex
  TEST_ASSERT_EQUAL_STRING("?\r\r", setReply.text.c_str());

  ElmReply readReply = engine.execute("AT@2");
  TEST_ASSERT_EQUAL_STRING("FFFFFFFFFFFF\r\r", readReply.text.c_str());
}

// --- ATRD / ATSDhh -----------------------------------------------------------

void test_atrd_default_is_zero() {
  InMemorySettingsStore store;
  ElmCommandEngine engine(store);

  ElmReply reply = engine.execute("ATRD");
  TEST_ASSERT_EQUAL_STRING("00\r\r", reply.text.c_str());
}

void test_atsdhh_valid_round_trips_through_atrd() {
  InMemorySettingsStore store;
  ElmCommandEngine engine(store);

  ElmReply setReply = engine.execute("ATSD7B");
  TEST_ASSERT_EQUAL_STRING("OK\r\r", setReply.text.c_str());

  ElmReply readReply = engine.execute("ATRD");
  TEST_ASSERT_EQUAL_STRING("7B\r\r", readReply.text.c_str());
}

void test_atsdhh_invalid_is_rejected_and_byte_unchanged() {
  InMemorySettingsStore store;
  ElmCommandEngine engine(store);
  engine.execute("ATSD7B");

  ElmReply setReply = engine.execute("ATSDZZ");  // not hex
  TEST_ASSERT_EQUAL_STRING("?\r\r", setReply.text.c_str());

  ElmReply readReply = engine.execute("ATRD");
  TEST_ASSERT_EQUAL_STRING("7B\r\r", readReply.text.c_str());
}

// --- ATM0/ATM1 gating: does a change reach the store? ------------------------

void test_m0_blocks_the_store_write_but_still_applies_this_session() {
  InMemorySettingsStore store;
  ElmCommandEngine engine(store);

  engine.execute("ATM0");
  engine.execute("AT@3AABBCCDDEEFF");

  // Takes effect for the current session...
  ElmReply sameSessionRead = engine.execute("AT@2");
  TEST_ASSERT_EQUAL_STRING("AABBCCDDEEFF\r\r", sameSessionRead.text.c_str());

  // ...but never reached the store: a fresh engine over the same store
  // (simulating a reconnect/power cycle) still sees the old value.
  ElmCommandEngine freshEngine(store);
  ElmReply freshRead = freshEngine.execute("AT@2");
  TEST_ASSERT_EQUAL_STRING("FFFFFFFFFFFF\r\r", freshRead.text.c_str());
}

void test_m1_default_persists_across_a_fresh_engine() {
  InMemorySettingsStore store;
  ElmCommandEngine engine(store);

  engine.execute("AT@3AABBCCDDEEFF");  // M1 is the default: no ATM0 needed

  ElmCommandEngine freshEngine(store);
  ElmReply freshRead = freshEngine.execute("AT@2");
  TEST_ASSERT_EQUAL_STRING("AABBCCDDEEFF\r\r", freshRead.text.c_str());
}

void test_m1_after_m0_persists_the_next_change() {
  InMemorySettingsStore store;
  ElmCommandEngine engine(store);

  engine.execute("ATM0");
  engine.execute("AT@3AABBCCDDEEFF");  // blocked from the store
  engine.execute("ATM1");
  engine.execute("ATSD7B");  // now persisting again

  ElmCommandEngine freshEngine(store);
  TEST_ASSERT_EQUAL_STRING("FFFFFFFFFFFF\r\r", freshEngine.execute("AT@2").text.c_str());
  TEST_ASSERT_EQUAL_STRING("7B\r\r", freshEngine.execute("ATRD").text.c_str());
}

// --- ATFE ---------------------------------------------------------------------

void test_atfe_restores_factory_defaults_immediately_and_in_the_store() {
  InMemorySettingsStore store;
  ElmCommandEngine engine(store);
  engine.execute("AT@3AABBCCDDEEFF");
  engine.execute("ATSD7B");

  ElmReply feReply = engine.execute("ATFE");
  TEST_ASSERT_EQUAL_STRING("OK\r\r", feReply.text.c_str());

  TEST_ASSERT_EQUAL_STRING("FFFFFFFFFFFF\r\r", engine.execute("AT@2").text.c_str());
  TEST_ASSERT_EQUAL_STRING("00\r\r", engine.execute("ATRD").text.c_str());

  // A fresh session over the same (now-erased) store also sees defaults --
  // no power cycle needed to observe the reset.
  ElmCommandEngine freshEngine(store);
  TEST_ASSERT_EQUAL_STRING("FFFFFFFFFFFF\r\r", freshEngine.execute("AT@2").text.c_str());
  TEST_ASSERT_EQUAL_STRING("00\r\r", freshEngine.execute("ATRD").text.c_str());
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_at2_returns_default_device_id);
  RUN_TEST(test_at3_valid_twelve_hex_digits_is_stored_and_read_back);
  RUN_TEST(test_at3_wrong_length_is_rejected_and_id_unchanged);
  RUN_TEST(test_at3_invalid_hex_is_rejected_and_id_unchanged);
  RUN_TEST(test_atrd_default_is_zero);
  RUN_TEST(test_atsdhh_valid_round_trips_through_atrd);
  RUN_TEST(test_atsdhh_invalid_is_rejected_and_byte_unchanged);
  RUN_TEST(test_m0_blocks_the_store_write_but_still_applies_this_session);
  RUN_TEST(test_m1_default_persists_across_a_fresh_engine);
  RUN_TEST(test_m1_after_m0_persists_the_next_change);
  RUN_TEST(test_atfe_restores_factory_defaults_immediately_and_in_the_store);
  return UNITY_END();
}
