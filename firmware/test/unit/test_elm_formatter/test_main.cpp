#include <unity.h>

#include "../../support/can_frame_builder.h"
#include "elm/elm_formatter.h"

using namespace esp_obd::elm;
using namespace esp_obd::can;

// The worked example from docs/ELM_COMMAND_BEHAVIOR.md section 1.4: a
// single-frame response with PCI 0x06 (single frame, length 6), padded to
// 8 bytes on the wire.
CanFrame sampleRawFrame() {
  return CanFrameBuilder::standard(0x7E8)
      .data({0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00});
}
const uint8_t kSamplePayload[] = {0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10};
constexpr size_t kSamplePayloadLen = 6;

void setUp() {}
void tearDown() {}

void test_headers_off_caf1_spaces_on_prints_only_payload() {
  ElmSession session;
  session.headersEnabled = false;
  session.spacesEnabled = true;

  ElmReplyText body =
      formatResponseBody(session, sampleRawFrame(), kSamplePayload, kSamplePayloadLen, 8);
  TEST_ASSERT_EQUAL_STRING("41 00 BE 1F B8 10", body.c_str());
}

void test_headers_off_caf1_spaces_off_has_no_separators() {
  ElmSession session;
  session.headersEnabled = false;
  session.spacesEnabled = false;

  ElmReplyText body =
      formatResponseBody(session, sampleRawFrame(), kSamplePayload, kSamplePayloadLen, 8);
  TEST_ASSERT_EQUAL_STRING("4100BE1FB810", body.c_str());
}

void test_headers_on_dlc_off_trims_to_pci_declared_length() {
  ElmSession session;
  session.headersEnabled = true;
  session.displayDlcEnabled = false;
  session.spacesEnabled = true;

  // ATD0: automatic formatting trims CAN-level padding even with headers
  // on, so only 7 raw bytes (PCI + 6 payload bytes) are shown, not all 8.
  ElmReplyText body = formatResponseBody(session, sampleRawFrame(), kSamplePayload,
                                          kSamplePayloadLen, /*rawBytesToShow=*/7);
  TEST_ASSERT_EQUAL_STRING("7E8 06 41 00 BE 1F B8 10", body.c_str());
}

void test_headers_on_dlc_on_shows_full_raw_frame() {
  ElmSession session;
  session.headersEnabled = true;
  session.displayDlcEnabled = true;
  session.spacesEnabled = true;

  // ATD1: full 8 raw bytes including the trailing pad byte, plus DLC "8".
  ElmReplyText body = formatResponseBody(session, sampleRawFrame(), kSamplePayload,
                                          kSamplePayloadLen, /*rawBytesToShow=*/8);
  TEST_ASSERT_EQUAL_STRING("7E8 8 06 41 00 BE 1F B8 10 00", body.c_str());
}

void test_response_ending_l0_default() {
  ElmSession session;  // linefeedsEnabled = false (default)
  TEST_ASSERT_EQUAL_STRING("\r\r", responseEnding(session));
}

void test_response_ending_l1() {
  ElmSession session;
  session.linefeedsEnabled = true;
  TEST_ASSERT_EQUAL_STRING("\r\n\r\n", responseEnding(session));
}

void test_text_reply_appends_current_ending() {
  ElmSession session;
  session.linefeedsEnabled = true;
  ElmReply reply = textReply(session, "OK");
  TEST_ASSERT_EQUAL_STRING("OK\r\n\r\n", reply.text.c_str());
  TEST_ASSERT_EQUAL(static_cast<int>(ElmReplyKind::Text), static_cast<int>(reply.kind));
  TEST_ASSERT_TRUE(reply.appendPrompt);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_headers_off_caf1_spaces_on_prints_only_payload);
  RUN_TEST(test_headers_off_caf1_spaces_off_has_no_separators);
  RUN_TEST(test_headers_on_dlc_off_trims_to_pci_declared_length);
  RUN_TEST(test_headers_on_dlc_on_shows_full_raw_frame);
  RUN_TEST(test_response_ending_l0_default);
  RUN_TEST(test_response_ending_l1);
  RUN_TEST(test_text_reply_appends_current_ending);
  return UNITY_END();
}
