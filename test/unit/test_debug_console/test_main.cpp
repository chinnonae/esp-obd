#include <unity.h>

#include <cstdio>

#include "app/debug_console.h"

using namespace esp_obd::app;

void setUp() {}
void tearDown() {}

void test_help() {
  DebugCommand cmd = parseDebugCommand("#HELP");
  TEST_ASSERT_EQUAL(static_cast<int>(DebugCommandKind::Help), static_cast<int>(cmd.kind));
}

void test_status() {
  DebugCommand cmd = parseDebugCommand("#STATUS");
  TEST_ASSERT_EQUAL(static_cast<int>(DebugCommandKind::Status), static_cast<int>(cmd.kind));
}

void test_reboot() {
  DebugCommand cmd = parseDebugCommand("#REBOOT");
  TEST_ASSERT_EQUAL(static_cast<int>(DebugCommandKind::Reboot), static_cast<int>(cmd.kind));
}

void test_dbg_level_0_to_3() {
  for (int level = 0; level <= 3; ++level) {
    char line[8];
    std::snprintf(line, sizeof(line), "#DBG %d", level);
    DebugCommand cmd = parseDebugCommand(line);
    TEST_ASSERT_EQUAL(static_cast<int>(DebugCommandKind::SetDebugLevel),
                       static_cast<int>(cmd.kind));
    TEST_ASSERT_EQUAL(level, cmd.debugLevel);
  }
}

void test_dbg_level_out_of_range_is_unknown() {
  DebugCommand cmd = parseDebugCommand("#DBG 4");
  TEST_ASSERT_EQUAL(static_cast<int>(DebugCommandKind::Unknown), static_cast<int>(cmd.kind));
}

void test_dbg_non_numeric_is_unknown() {
  DebugCommand cmd = parseDebugCommand("#DBG x");
  TEST_ASSERT_EQUAL(static_cast<int>(DebugCommandKind::Unknown), static_cast<int>(cmd.kind));
}

void test_unknown_command() {
  DebugCommand cmd = parseDebugCommand("#BOGUS");
  TEST_ASSERT_EQUAL(static_cast<int>(DebugCommandKind::Unknown), static_cast<int>(cmd.kind));
}

void test_case_sensitive_does_not_match_lowercase() {
  // Unlike ELM commands, debug commands are exact/case-sensitive -- this
  // is part of how UART commands stay structurally unable to be parsed as
  // ELM commands (which are case-insensitive and never start with '#').
  DebugCommand cmd = parseDebugCommand("#help");
  TEST_ASSERT_EQUAL(static_cast<int>(DebugCommandKind::Unknown), static_cast<int>(cmd.kind));
}

void test_prefix_debug_line() {
  DebugLine line = prefixDebugLine("hello");
  TEST_ASSERT_EQUAL_STRING("#DBG: hello", line.c_str());
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_help);
  RUN_TEST(test_status);
  RUN_TEST(test_reboot);
  RUN_TEST(test_dbg_level_0_to_3);
  RUN_TEST(test_dbg_level_out_of_range_is_unknown);
  RUN_TEST(test_dbg_non_numeric_is_unknown);
  RUN_TEST(test_unknown_command);
  RUN_TEST(test_case_sensitive_does_not_match_lowercase);
  RUN_TEST(test_prefix_debug_line);
  return UNITY_END();
}
