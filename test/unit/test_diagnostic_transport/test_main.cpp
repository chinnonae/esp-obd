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

  transport.poll(200);  // overall deadline reached
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
  transport.poll(200);

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

int main(int argc, char** argv) {
  UNITY_BEGIN();
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
