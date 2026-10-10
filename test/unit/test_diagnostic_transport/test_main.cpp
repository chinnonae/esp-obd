#include <unity.h>

#include <array>

#include "../../support/fake_can_port.h"
#include "diagnostic/diagnostic_transport.h"

using namespace esp_obd::can;
using namespace esp_obd::diagnostic;

void setUp() {}
void tearDown() {}

namespace {

CanFrame singleFrameResponse(uint32_t id, std::array<uint8_t, 8> data, bool extended = false) {
  return extended ? *makeExtendedFrame(id, data, 8) : *makeStandardFrame(id, data, 8);
}

DiagnosticRequest physicalRequest(uint32_t requestId, const uint8_t* payload, size_t len) {
  DiagnosticRequest request;
  request.requestId = requestId;
  request.payload = payload;
  request.payloadLength = len;
  request.responseTimeoutMs = 200;
  return request;
}

DiagnosticRequest functionalRequest(const uint8_t* payload, size_t len) {
  return physicalRequest(obd::kFunctionalRequestId11Bit, payload, len);
}

}  // namespace

// --- Physical request: stop at first complete response ----------------------

void test_physical_request_completes_on_first_responder() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x0C};
  auto request = physicalRequest(0x7E0, payload, 2);

  transport.start(0, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  TEST_ASSERT_FALSE(transport.finished());  // sent; now waiting for a responder

  port.queueRx(singleFrameResponse(0x7E8, {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10, 0x00}));
  transport.poll(10);

  TEST_ASSERT_TRUE(transport.finished());
  const DiagnosticResult& result = transport.result();
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticOutcome::Complete), static_cast<int>(result.outcome));
  TEST_ASSERT_EQUAL(1, result.responderCount);
  TEST_ASSERT_EQUAL_UINT32(0x7E8, result.responders[0].sourceId);
  TEST_ASSERT_EQUAL(6, result.responders[0].payloadLength);
  const uint8_t expected[] = {0x41, 0x00, 0xBE, 0x1F, 0xB8, 0x10};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, result.responders[0].payload.data(), 6);
  TEST_ASSERT_TRUE(result.responders[0].rawFrameCount >= 1);
}

void test_physical_request_ignores_a_second_responder() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x0C};
  auto request = physicalRequest(0x7E0, payload, 2);

  transport.start(0, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));
  port.queueRx(singleFrameResponse(0x7E9, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));

  transport.poll(10);  // drains only the first queued frame, then finalizes

  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(1, transport.result().responderCount);
}

// --- Functional request: collects distinct responders until timeout ---------

void test_functional_request_collects_two_distinct_responders() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x00};
  auto request = functionalRequest(payload, 2);

  transport.start(0, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));
  port.queueRx(singleFrameResponse(0x7E9, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));

  transport.poll(10);   // drains frame 1
  TEST_ASSERT_FALSE(transport.finished());  // functional: no explicit count, waits out timeout
  transport.poll(20);   // drains frame 2
  TEST_ASSERT_FALSE(transport.finished());

  // The timer restarts after each response (as on an ELM327): 20 + 200.
  transport.poll(219);
  TEST_ASSERT_FALSE(transport.finished());
  transport.poll(220);
  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticOutcome::Complete),
                     static_cast<int>(transport.result().outcome));
  TEST_ASSERT_EQUAL(2, transport.result().responderCount);
}

void test_explicit_response_count_stops_collection_early() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x00};
  auto request = functionalRequest(payload, 2);
  request.maxResponses = 1;  // e.g. "01001": stop after 1 complete responder

  transport.start(0, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));
  port.queueRx(singleFrameResponse(0x7E9, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));

  transport.poll(10);  // one complete responder reaches the requested count

  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(1, transport.result().responderCount);
}

// --- Filtering (ATCRA) --------------------------------------------------------

void test_explicit_receive_address_filters_out_other_responders() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x00};
  auto request = functionalRequest(payload, 2);
  request.explicitReceiveAddress = 0x7E9;

  transport.start(0, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));  // filtered out
  port.queueRx(singleFrameResponse(0x7E9, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));  // accepted

  transport.poll(10);
  transport.poll(20);
  transport.poll(220);  // 200ms after the accepted frame

  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(1, transport.result().responderCount);
  TEST_ASSERT_EQUAL_UINT32(0x7E9, transport.result().responders[0].sourceId);
}

// --- No data / bus error ------------------------------------------------------

void test_no_response_yields_no_data() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x0C};
  auto request = physicalRequest(0x7E0, payload, 2);

  transport.start(0, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  transport.poll(200);

  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticOutcome::NoData),
                     static_cast<int>(transport.result().outcome));
  TEST_ASSERT_EQUAL(0, transport.result().responderCount);
}

void test_transmit_failure_yields_bus_error_before_any_rx() {
  FakeCanPort port;
  port.setNextSendResult(CanResult::BusError);
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x0C};
  auto request = physicalRequest(0x7E0, payload, 2);

  transport.start(0, request, obd::ObdCanProtocol::Iso15765_11bit_500k);

  TEST_ASSERT_TRUE(transport.finished());  // fails synchronously: SF send is immediate
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticOutcome::BusError),
                     static_cast<int>(transport.result().outcome));
}

// --- Auto-search ---------------------------------------------------------------

void test_auto_search_succeeds_on_29bit_after_11bit_silence() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x00};
  DiagnosticRequest request;
  request.payload = payload;
  request.payloadLength = 2;
  request.responseTimeoutMs = 200;

  transport.startAutoSearch(0, request);  // tries SP6 (11-bit/500k) first
  TEST_ASSERT_FALSE(transport.finished());

  transport.poll(199);
  TEST_ASSERT_FALSE(transport.finished());
  transport.poll(200);  // SP6 silent: advances to SP7 (29-bit/500k)
  TEST_ASSERT_FALSE(transport.finished());

  // Only queue the 29-bit response now that the search has moved on to it --
  // it would otherwise be drained and discarded while SP6 was still active.
  port.queueRx(singleFrameResponse(0x18DAF110, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}, /*extended=*/true));
  transport.poll(210);

  TEST_ASSERT_TRUE(transport.finished());
  const DiagnosticResult& result = transport.result();
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticOutcome::Complete), static_cast<int>(result.outcome));
  TEST_ASSERT_TRUE(result.connectedProtocol.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(obd::ObdCanProtocol::Iso15765_29bit_500k),
                     static_cast<int>(*result.connectedProtocol));
  TEST_ASSERT_EQUAL(1, result.responderCount);
  TEST_ASSERT_EQUAL_UINT32(0x18DAF110, result.responders[0].sourceId);
}

void test_auto_search_exhaustion_yields_unable_to_connect() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x00};
  DiagnosticRequest request;
  request.payload = payload;
  request.payloadLength = 2;
  request.responseTimeoutMs = 200;

  transport.startAutoSearch(0, request);
  transport.poll(200);  // SP6 -> SP7
  transport.poll(400);  // SP7 -> SP8
  transport.poll(600);  // SP8 -> SP9
  TEST_ASSERT_FALSE(transport.finished());
  transport.poll(800);  // SP9 silent too: all four exhausted

  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticOutcome::UnableToConnect),
                     static_cast<int>(transport.result().outcome));
}

// --- "Response pending" (7F xx 78) --------------------------------------------

void test_response_pending_waits_for_the_real_answer_past_the_normal_timeout() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x22, 0xF1, 0x90};
  auto request = physicalRequest(0x7E0, payload, 3);

  transport.start(0, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x03, 0x7F, 0x22, 0x78, 0, 0, 0, 0}));
  transport.poll(10);
  TEST_ASSERT_FALSE(transport.finished());  // pending is not an answer

  transport.poll(300);  // past the 200 ms ATST timeout, inside P2*
  TEST_ASSERT_FALSE(transport.finished());

  port.queueRx(singleFrameResponse(0x7E8, {0x04, 0x62, 0xF1, 0x90, 0x01, 0, 0, 0}));
  transport.poll(400);

  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticOutcome::Complete),
                    static_cast<int>(transport.result().outcome));
  TEST_ASSERT_EQUAL(1, transport.result().responderCount);
  const Responder& r = transport.result().responders[0];
  TEST_ASSERT_EQUAL(4, r.payloadLength);
  TEST_ASSERT_EQUAL_HEX8(0x62, r.payload[0]);
}

void test_response_pending_with_no_follow_up_ends_in_no_data_after_p2_star() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x22, 0xF1, 0x90};
  auto request = physicalRequest(0x7E0, payload, 3);

  transport.start(0, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x03, 0x7F, 0x22, 0x78, 0, 0, 0, 0}));
  transport.poll(10);

  transport.poll(10 + kResponsePendingTimeoutMs - 1);
  TEST_ASSERT_FALSE(transport.finished());
  transport.poll(10 + kResponsePendingTimeoutMs);
  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticOutcome::NoData),
                    static_cast<int>(transport.result().outcome));
}

void test_other_negative_responses_still_complete_immediately() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x22, 0xF1, 0x90};
  auto request = physicalRequest(0x7E0, payload, 3);

  transport.start(0, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x03, 0x7F, 0x22, 0x31, 0, 0, 0, 0}));
  transport.poll(10);
  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(1, transport.result().responderCount);
}

// --- Stale frames ------------------------------------------------------------

void test_each_request_discards_frames_already_queued_once() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x0C};

  transport.start(0, physicalRequest(0x7E0, payload, 2), obd::ObdCanProtocol::Iso15765_11bit_500k);
  TEST_ASSERT_EQUAL(1, port.discardCount());

  transport.startAutoSearch(0, functionalRequest(payload, 2));
  TEST_ASSERT_EQUAL(2, port.discardCount());
}

// --- Adaptive timing (ATAT1/ATAT2) ------------------------------------------

namespace {
// One functional request answered by 7E8 at +10ms and 7E9 at +15ms after TX,
// collected to completion. Teaches the transport a 15ms latency estimate.
void runTwoResponderRequest(FakeCanPort& port, DiagnosticTransport& transport,
                            const DiagnosticRequest& request, Milliseconds start) {
  transport.start(start, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));
  transport.poll(start + 10);
  port.queueRx(singleFrameResponse(0x7E9, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));
  transport.poll(start + 15);
  for (Milliseconds t = start + 16; !transport.finished() && t < start + 1000; ++t) {
    transport.poll(t);
  }
  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(2, transport.result().responderCount);
}

// Polls 1ms at a time from `from` and returns when the transport finished.
Milliseconds finishTime(DiagnosticTransport& transport, Milliseconds from) {
  Milliseconds t = from;
  while (!transport.finished() && t < from + 1000) {
    transport.poll(++t);
  }
  return t;
}
}  // namespace

void test_adaptive_normal_shortens_the_wait_after_responses_once_learned() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x00};
  auto request = functionalRequest(payload, 2);
  request.adaptiveTiming = AdaptiveTimingMode::Normal;

  // First request: nothing learned yet, so the full 200ms restart applies.
  runTwoResponderRequest(port, transport, request, 0);

  // Second request: estimate 15ms -> wait until TX + 2 x 15 = 30ms.
  transport.start(1000, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));
  transport.poll(1010);
  TEST_ASSERT_EQUAL(1030, finishTime(transport, 1010));
  TEST_ASSERT_EQUAL(1, transport.result().responderCount);
}

void test_adaptive_aggressive_uses_a_tighter_window() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x00};
  auto request = functionalRequest(payload, 2);
  request.adaptiveTiming = AdaptiveTimingMode::Aggressive;
  runTwoResponderRequest(port, transport, request, 0);

  // 1.5 x 15 = 22ms after TX, but at least 8ms after the frame at +10.
  transport.start(1000, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));
  transport.poll(1010);
  TEST_ASSERT_EQUAL(1022, finishTime(transport, 1010));
}

void test_adaptive_off_always_waits_the_full_restart() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x00};
  auto request = functionalRequest(payload, 2);
  request.adaptiveTiming = AdaptiveTimingMode::Off;
  runTwoResponderRequest(port, transport, request, 0);

  transport.start(1000, request, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));
  transport.poll(1010);
  TEST_ASSERT_EQUAL(1210, finishTime(transport, 1010));
}

void test_adaptive_never_cuts_off_a_multi_frame_response_in_progress() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x00};
  auto request = functionalRequest(payload, 2);
  request.adaptiveTiming = AdaptiveTimingMode::Normal;
  runTwoResponderRequest(port, transport, request, 0);  // window now ~30ms

  const uint8_t vin[] = {0x09, 0x02};
  auto vinRequest = functionalRequest(vin, 2);
  vinRequest.adaptiveTiming = AdaptiveTimingMode::Normal;
  transport.start(1000, vinRequest, obd::ObdCanProtocol::Iso15765_11bit_500k);
  port.queueRx(singleFrameResponse(0x7E8, {0x10, 0x0A, 0x49, 0x02, 0x01, 0x31, 0x32, 0x33}));
  transport.poll(1010);
  transport.poll(1035);  // past the adaptive window, but a CF is still due
  TEST_ASSERT_FALSE(transport.finished());
  port.queueRx(singleFrameResponse(0x7E8, {0x21, 0x34, 0x35, 0x36, 0x37, 0, 0, 0}));
  transport.poll(1040);
  finishTime(transport, 1040);

  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(1, transport.result().responderCount);
  TEST_ASSERT_EQUAL(10, transport.result().responders[0].payloadLength);
}

// --- Abort -----------------------------------------------------------------

void test_abort_finishes_with_no_data_and_ignores_later_frames() {
  FakeCanPort port;
  DiagnosticTransport transport(port);
  const uint8_t payload[] = {0x01, 0x00};
  transport.start(0, functionalRequest(payload, 2), obd::ObdCanProtocol::Iso15765_11bit_500k);

  transport.abort();
  TEST_ASSERT_TRUE(transport.finished());
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticOutcome::NoData),
                    static_cast<int>(transport.result().outcome));

  port.queueRx(singleFrameResponse(0x7E8, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}));
  transport.poll(10);
  TEST_ASSERT_EQUAL(0, transport.result().responderCount);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_response_pending_waits_for_the_real_answer_past_the_normal_timeout);
  RUN_TEST(test_response_pending_with_no_follow_up_ends_in_no_data_after_p2_star);
  RUN_TEST(test_other_negative_responses_still_complete_immediately);
  RUN_TEST(test_each_request_discards_frames_already_queued_once);
  RUN_TEST(test_adaptive_normal_shortens_the_wait_after_responses_once_learned);
  RUN_TEST(test_adaptive_aggressive_uses_a_tighter_window);
  RUN_TEST(test_adaptive_off_always_waits_the_full_restart);
  RUN_TEST(test_adaptive_never_cuts_off_a_multi_frame_response_in_progress);
  RUN_TEST(test_abort_finishes_with_no_data_and_ignores_later_frames);
  RUN_TEST(test_physical_request_completes_on_first_responder);
  RUN_TEST(test_physical_request_ignores_a_second_responder);
  RUN_TEST(test_functional_request_collects_two_distinct_responders);
  RUN_TEST(test_explicit_response_count_stops_collection_early);
  RUN_TEST(test_explicit_receive_address_filters_out_other_responders);
  RUN_TEST(test_no_response_yields_no_data);
  RUN_TEST(test_transmit_failure_yields_bus_error_before_any_rx);
  RUN_TEST(test_auto_search_succeeds_on_29bit_after_11bit_silence);
  RUN_TEST(test_auto_search_exhaustion_yields_unable_to_connect);
  return UNITY_END();
}
