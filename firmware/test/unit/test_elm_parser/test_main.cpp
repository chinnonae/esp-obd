#include <unity.h>

#include "elm/elm_parser.h"

using namespace esp_obd::elm;

void setUp() {}
void tearDown() {}

void test_normalize_strips_spaces_and_uppercases() {
  NormalizedLine line = normalizeLine("at sh 7e0");
  TEST_ASSERT_EQUAL_STRING("ATSH7E0", line.c_str());
}

void test_normalize_is_idempotent_on_already_normalized_input() {
  NormalizedLine line = normalizeLine("ATSH7E0");
  TEST_ASSERT_EQUAL_STRING("ATSH7E0", line.c_str());
}

void test_starts_with_at() {
  TEST_ASSERT_TRUE(startsWithAt(normalizeLine("ATZ")));
  TEST_ASSERT_FALSE(startsWithAt(normalizeLine("0100")));
  TEST_ASSERT_FALSE(startsWithAt(normalizeLine("A")));  // too short to be "AT"
}

// --- Hex request validation (docs/ELM_COMMAND_BEHAVIOR.md section 2.5) ---

void test_even_length_hex_is_valid_with_no_response_count() {
  auto result = validateHexRequestLine(normalizeLine("0100"), /*automaticFormattingEnabled=*/true);
  TEST_ASSERT_TRUE(result.valid);
  TEST_ASSERT_EQUAL_STRING("0100", result.payloadHex.c_str());
  TEST_ASSERT_FALSE(result.maxResponses.has_value());
}

void test_odd_length_ending_nonzero_sets_max_responses() {
  // "01002" == mode/PID "0100", stop after 2 complete responders.
  auto result = validateHexRequestLine(normalizeLine("01002"), /*automaticFormattingEnabled=*/true);
  TEST_ASSERT_TRUE(result.valid);
  TEST_ASSERT_EQUAL_STRING("0100", result.payloadHex.c_str());
  TEST_ASSERT_TRUE(result.maxResponses.has_value());
  TEST_ASSERT_EQUAL_UINT8(2, *result.maxResponses);
}

void test_odd_length_ending_zero_is_rejected() {
  auto result = validateHexRequestLine(normalizeLine("01000"), /*automaticFormattingEnabled=*/true);
  TEST_ASSERT_FALSE(result.valid);
}

void test_invalid_hex_characters_rejected() {
  auto result = validateHexRequestLine(normalizeLine("01GH"), /*automaticFormattingEnabled=*/true);
  TEST_ASSERT_FALSE(result.valid);
}

void test_response_count_nibble_with_no_payload_is_rejected() {
  auto result = validateHexRequestLine(normalizeLine("5"), /*automaticFormattingEnabled=*/true);
  TEST_ASSERT_FALSE(result.valid);
}

void test_caf0_rejects_payload_over_eight_bytes() {
  // 9 bytes = 18 hex chars, over the CAF0 (raw) 8-byte cap.
  auto result = validateHexRequestLine(normalizeLine("010203040506070809"),
                                        /*automaticFormattingEnabled=*/false);
  TEST_ASSERT_FALSE(result.valid);
}

void test_caf1_allows_payload_over_eight_bytes() {
  auto result = validateHexRequestLine(normalizeLine("010203040506070809"),
                                        /*automaticFormattingEnabled=*/true);
  TEST_ASSERT_TRUE(result.valid);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_normalize_strips_spaces_and_uppercases);
  RUN_TEST(test_normalize_is_idempotent_on_already_normalized_input);
  RUN_TEST(test_starts_with_at);
  RUN_TEST(test_even_length_hex_is_valid_with_no_response_count);
  RUN_TEST(test_odd_length_ending_nonzero_sets_max_responses);
  RUN_TEST(test_odd_length_ending_zero_is_rejected);
  RUN_TEST(test_invalid_hex_characters_rejected);
  RUN_TEST(test_response_count_nibble_with_no_payload_is_rejected);
  RUN_TEST(test_caf0_rejects_payload_over_eight_bytes);
  RUN_TEST(test_caf1_allows_payload_over_eight_bytes);
  return UNITY_END();
}
