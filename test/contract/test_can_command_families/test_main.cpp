#include <unity.h>

#include "../../support/fake_can_port.h"
#include "../../support/in_memory_settings_store.h"
#include "app/elm_application.h"

using namespace esp_obd;
using namespace esp_obd::can;
using namespace esp_obd::elm;

void setUp() {}
void tearDown() {}

// --- 2.2 Protocol and timeout ------------------------------------------------

void test_sp0_selects_automatic_search() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);

  ElmReply reply = app.execute(0, "ATSP0");
  TEST_ASSERT_EQUAL_STRING("OK\r\r", reply.text.c_str());
  TEST_ASSERT_EQUAL(static_cast<int>(ElmProtocol::AutomaticSearch),
                     static_cast<int>(app.engine().session().protocol));
  TEST_ASSERT_FALSE(app.engine().session().protocolConnected);
  TEST_ASSERT_EQUAL_UINT32(0x7DF, app.engine().session().requestId);
}

void test_sp6_through_sp9_select_protocol_and_default_request_id() {
  struct Case {
    const char* cmd;
    ElmProtocol protocol;
    uint32_t requestId;
  };
  const Case cases[] = {
      {"ATSP6", ElmProtocol::Iso15765_11bit_500k, 0x7DF},
      {"ATSP7", ElmProtocol::Iso15765_29bit_500k, 0x18DB33F1},
      {"ATSP8", ElmProtocol::Iso15765_11bit_250k, 0x7DF},
      {"ATSP9", ElmProtocol::Iso15765_29bit_250k, 0x18DB33F1},
  };
  for (const auto& c : cases) {
    FakeCanPort port;
    InMemorySettingsStore store;
    app::ElmApplication app(port, store);
    ElmReply reply = app.execute(0, c.cmd);
    TEST_ASSERT_EQUAL_STRING("OK\r\r", reply.text.c_str());
    TEST_ASSERT_EQUAL(static_cast<int>(c.protocol), static_cast<int>(app.engine().session().protocol));
    TEST_ASSERT_TRUE(app.engine().session().protocolConnected);
    TEST_ASSERT_EQUAL_UINT32(c.requestId, app.engine().session().requestId);
  }
}

void test_sp1_through_5_and_spa_through_c_are_unsupported() {
  const char* unsupported[] = {"ATSP1", "ATSP2", "ATSP3", "ATSP4", "ATSP5", "ATSPA", "ATSPB", "ATSPC"};
  for (const char* cmd : unsupported) {
    FakeCanPort port;
    InMemorySettingsStore store;
    app::ElmApplication app(port, store);
    ElmSession before = app.engine().session();
    ElmReply reply = app.execute(0, cmd);
    TEST_ASSERT_EQUAL_STRING("?\r\r", reply.text.c_str());
    TEST_ASSERT_TRUE(before == app.engine().session());
  }
}

void test_tp_and_tpa_select_protocol_like_sp() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATTPA7");
  TEST_ASSERT_EQUAL(static_cast<int>(ElmProtocol::Iso15765_29bit_500k),
                     static_cast<int>(app.engine().session().protocol));
  TEST_ASSERT_TRUE(app.engine().session().protocolConnected);
}

void test_atdp_and_atdpn_reflect_protocol_state() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);

  TEST_ASSERT_EQUAL_STRING("AUTO\r\r", app.execute(0, "ATDP").text.c_str());
  TEST_ASSERT_EQUAL_STRING("0\r\r", app.execute(0, "ATDPN").text.c_str());

  app.execute(0, "ATSP6");
  TEST_ASSERT_EQUAL_STRING("ISO 15765-4 (CAN 11/500)\r\r", app.execute(0, "ATDP").text.c_str());
  TEST_ASSERT_EQUAL_STRING("6\r\r", app.execute(0, "ATDPN").text.c_str());
}

void test_atdp_shows_auto_prefix_only_after_auto_search_success() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);

  port.queueRx(*makeStandardFrame(0x7E8, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}, 8));
  app.execute(0, "01001");  // functional, capped at 1: triggers auto-search

  TEST_ASSERT_EQUAL_STRING("AUTO, ISO 15765-4 (CAN 11/500)\r\r", app.execute(0, "ATDP").text.c_str());
  TEST_ASSERT_EQUAL_STRING("A6\r\r", app.execute(0, "ATDPN").text.c_str());
}

void test_atsthh_sets_timeout_max_4_or_hhx4() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);

  app.execute(0, "ATST64");  // 0x64 = 100 -> 400ms
  TEST_ASSERT_EQUAL_UINT32(400, app.engine().session().responseTimeoutMs);

  app.execute(0, "ATST00");  // 0 -> floor of 4ms
  TEST_ASSERT_EQUAL_UINT32(4, app.engine().session().responseTimeoutMs);
}

void test_atat_sets_adaptive_timing_mode() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);

  app.execute(0, "ATAT0");
  TEST_ASSERT_EQUAL(static_cast<int>(AdaptiveTiming::Off), static_cast<int>(app.engine().session().adaptiveTiming));
  app.execute(0, "ATAT2");
  TEST_ASSERT_EQUAL(static_cast<int>(AdaptiveTiming::Mode2), static_cast<int>(app.engine().session().adaptiveTiming));
}

void test_atctm_sets_timeout_multiplier() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATCTM5");
  TEST_ASSERT_EQUAL_UINT8(5, app.engine().session().canTimeoutMultiplier);
}

void test_atpc_disconnects_and_leaves_monitor_mode() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATSP6");
  app.execute(0, "ATMA");
  TEST_ASSERT_TRUE(app.engine().session().protocolConnected);
  TEST_ASSERT_TRUE(app.engine().session().monitorActive);

  app.execute(0, "ATPC");
  TEST_ASSERT_FALSE(app.engine().session().protocolConnected);
  TEST_ASSERT_FALSE(app.engine().session().monitorActive);
  // Protocol selection itself is retained ("remains selected").
  TEST_ASSERT_EQUAL(static_cast<int>(ElmProtocol::Iso15765_11bit_500k),
                     static_cast<int>(app.engine().session().protocol));
}

// --- 2.3 Addressing and filtering --------------------------------------------

void test_atsh_sets_standard_and_extended_header_and_rejects_out_of_range() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);

  TEST_ASSERT_EQUAL_STRING("OK\r\r", app.execute(0, "ATSH7E0").text.c_str());
  TEST_ASSERT_EQUAL_UINT32(0x7E0, *app.engine().session().customHeaderId);

  TEST_ASSERT_EQUAL_STRING("?\r\r", app.execute(0, "ATSH800").text.c_str());  // > 0x7FF
  TEST_ASSERT_EQUAL_UINT32(0x7E0, *app.engine().session().customHeaderId);   // unchanged

  TEST_ASSERT_EQUAL_STRING("OK\r\r", app.execute(0, "ATSH18DB33F1").text.c_str());
  TEST_ASSERT_EQUAL_UINT32(0x18DB33F1, *app.engine().session().customHeaderId);

  TEST_ASSERT_EQUAL_STRING("?\r\r", app.execute(0, "ATSH20000000").text.c_str());  // > 0x1FFFFFFF
}

void test_atcp_sets_priority_bits() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATCP18");
  TEST_ASSERT_EQUAL_UINT8(0x18, app.engine().session().priorityBits);
}

void test_atcra_and_atar_clear_receive_address_and_filter() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATCRA7E8");
  app.execute(0, "ATCF7E8");
  TEST_ASSERT_TRUE(app.engine().session().receiveAddress.has_value());
  TEST_ASSERT_TRUE(app.engine().session().idFilter.has_value());

  app.execute(0, "ATAR");
  TEST_ASSERT_FALSE(app.engine().session().receiveAddress.has_value());
  TEST_ASSERT_FALSE(app.engine().session().idFilter.has_value());
}

void test_atcra_sets_exact_receive_address() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATCRA7E8");
  TEST_ASSERT_EQUAL_UINT32(0x7E8, *app.engine().session().receiveAddress);
}

void test_atcf_and_atcm_set_filter_and_mask_independently() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATCF7E8");
  app.execute(0, "ATCM7F8");
  TEST_ASSERT_EQUAL_UINT32(0x7E8, app.engine().session().idFilter->filterValue);
  TEST_ASSERT_EQUAL_UINT32(0x7F8, app.engine().session().idFilter->mask);
}

void test_atcaf_and_atcfc_toggles() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATCAF0");
  TEST_ASSERT_FALSE(app.engine().session().automaticFormattingEnabled);
  app.execute(0, "ATCFC0");
  TEST_ASSERT_FALSE(app.engine().session().automaticFlowControlEnabled);
}

void test_atfcsm_sets_flow_control_mode() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATFCSM1");
  TEST_ASSERT_EQUAL(static_cast<int>(FlowControlMode::ManualHeaderAndData),
                     static_cast<int>(app.engine().session().flowControlMode));
  app.execute(0, "ATFCSM2");
  TEST_ASSERT_EQUAL(static_cast<int>(FlowControlMode::ManualDataAutoHeader),
                     static_cast<int>(app.engine().session().flowControlMode));
}

void test_atfcsh_sets_manual_flow_control_id() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATFCSH7E0");
  TEST_ASSERT_EQUAL_UINT32(0x7E0, *app.engine().session().manualFlowControlId);
}

void test_atfcsd_sets_manual_flow_control_bytes() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATFCSD0102030405");
  TEST_ASSERT_EQUAL_UINT8(5, app.engine().session().manualFlowControlDataLen);
  const uint8_t expected[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, app.engine().session().manualFlowControlData.data(), 5);

  TEST_ASSERT_EQUAL_STRING("?\r\r", app.execute(0, "ATFCSDZZ").text.c_str());  // invalid hex
}

void test_atcea_atceahh_atcerhh() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);

  app.execute(0, "ATCEA1A");
  TEST_ASSERT_TRUE(app.engine().session().extendedAddressingEnabled);
  TEST_ASSERT_EQUAL_UINT8(0x1A, app.engine().session().extendedAddressByte);

  app.execute(0, "ATCER1B");
  TEST_ASSERT_EQUAL_UINT8(0x1B, *app.engine().session().requiredExtendedAddressByte);

  app.execute(0, "ATCEA");
  TEST_ASSERT_FALSE(app.engine().session().extendedAddressingEnabled);
}

// --- 2.4 Monitoring and CAN diagnostics ---------------------------------------

void test_atma_atmr_atmt_enter_monitor_mode_silently() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);

  ElmReply reply = app.execute(0, "ATMA");
  TEST_ASSERT_TRUE(reply.text.empty());
  TEST_ASSERT_FALSE(reply.appendPrompt);
  TEST_ASSERT_TRUE(app.monitorActive());
  TEST_ASSERT_EQUAL(static_cast<int>(MonitorMode::All), static_cast<int>(app.engine().session().monitorMode));

  app.execute(0, "ATMR5A");
  TEST_ASSERT_EQUAL(static_cast<int>(MonitorMode::ReceivedAddress),
                     static_cast<int>(app.engine().session().monitorMode));
  TEST_ASSERT_EQUAL_UINT8(0x5A, app.engine().session().monitorAddressByte);

  app.execute(0, "ATMT5A");
  TEST_ASSERT_EQUAL(static_cast<int>(MonitorMode::TransmittedAddress),
                     static_cast<int>(app.engine().session().monitorMode));
}

void test_atcs_reports_typed_status() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  ElmReply reply = app.execute(0, "ATCS");
  TEST_ASSERT_EQUAL_STRING("TXERR:00 RXERR:00 BUSOFF:0 RATE:500K\r\r", reply.text.c_str());
  TEST_ASSERT_EQUAL(0, port.transmitted().size());  // "no CAN TX"
}

void test_atcsm_toggles_silent_monitoring() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATCSM1");
  TEST_ASSERT_TRUE(app.engine().session().silentMonitoringEnabled);
  app.execute(0, "ATCSM0");
  TEST_ASSERT_FALSE(app.engine().session().silentMonitoringEnabled);
}

void test_atrtr_sends_remote_frame_with_current_header() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATSH7E0");

  ElmReply reply = app.execute(0, "ATRTR");
  TEST_ASSERT_EQUAL_STRING("OK\r\r", reply.text.c_str());
  TEST_ASSERT_EQUAL(1, port.transmitted().size());
  const CanFrame& frame = port.transmitted()[0];
  TEST_ASSERT_EQUAL_UINT32(0x7E0, frame.id);
  TEST_ASSERT_TRUE(frame.remoteRequest);
  TEST_ASSERT_EQUAL_UINT8(0, frame.dlc);
}

void test_atv_toggles_variable_dlc_flag() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATV1");
  TEST_ASSERT_TRUE(app.engine().session().variableDlcEnabled);
  app.execute(0, "ATV0");
  TEST_ASSERT_FALSE(app.engine().session().variableDlcEnabled);
}

void test_atbd_reports_no_data_then_the_last_accepted_frame() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);

  TEST_ASSERT_EQUAL_STRING("NO DATA\r\r", app.execute(0, "ATBD").text.c_str());

  app.execute(0, "ATSH7E0");
  port.queueRx(*makeStandardFrame(0x7E8, {0x02, 0x41, 0x00, 0, 0, 0, 0, 0}, 8));
  app.execute(0, "0100");  // physical (ATSH set): completes with one response

  ElmReply dump = app.execute(0, "ATBD");
  TEST_ASSERT_EQUAL_STRING("7E8 8 02 41 00 00 00 00 00 00\r\r", dump.text.c_str());
}

void test_monitor_all_prints_every_frame() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATMA");

  port.queueRx(*makeStandardFrame(0x123, {0x01, 0x02, 0x03, 0, 0, 0, 0, 0}, 3));
  ElmReplyText line = app.pollMonitor(0);
  TEST_ASSERT_EQUAL_STRING("01 02 03\r\r", line.c_str());
}

void test_monitor_received_address_filters_by_low_byte() {
  FakeCanPort port;
  InMemorySettingsStore store;
  app::ElmApplication app(port, store);
  app.execute(0, "ATMR5A");

  port.queueRx(*makeStandardFrame(0x111, {0xAA}, 1));  // low byte 0x11 != 0x5A
  ElmReplyText ignored = app.pollMonitor(0);
  TEST_ASSERT_TRUE(ignored.empty());

  port.queueRx(*makeStandardFrame(0x15A, {0xBB}, 1));  // low byte 0x5A matches
  ElmReplyText matched = app.pollMonitor(0);
  TEST_ASSERT_EQUAL_STRING("BB\r\r", matched.c_str());
}

// --- Section 3: explicitly unsupported commands -------------------------------

void test_unsupported_commands_return_unknown_without_side_effects() {
  const char* unsupported[] = {
      "ATFI",   "ATII",   "ATSI",   "ATWM1",  // non-CAN protocols
      "ATJE",   "ATJS",   "ATDM1",  "ATMP1",  // J1939
      "ATRV",   "ATIGN",  "ATCV0",            // voltage/ignition
      "ATBRD1", "ATLP",                       // serial-rate/power
      "ATPPA1", "ATAM0",  "ATBI",              // programmable params / other
  };
  for (const char* cmd : unsupported) {
    FakeCanPort port;
    InMemorySettingsStore store;
    app::ElmApplication app(port, store);
    ElmSession before = app.engine().session();

    ElmReply reply = app.execute(0, cmd);

    TEST_ASSERT_EQUAL_STRING("?\r\r", reply.text.c_str());
    TEST_ASSERT_TRUE(before == app.engine().session());
    TEST_ASSERT_EQUAL(0, port.transmitted().size());
  }
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_sp0_selects_automatic_search);
  RUN_TEST(test_sp6_through_sp9_select_protocol_and_default_request_id);
  RUN_TEST(test_sp1_through_5_and_spa_through_c_are_unsupported);
  RUN_TEST(test_tp_and_tpa_select_protocol_like_sp);
  RUN_TEST(test_atdp_and_atdpn_reflect_protocol_state);
  RUN_TEST(test_atdp_shows_auto_prefix_only_after_auto_search_success);
  RUN_TEST(test_atsthh_sets_timeout_max_4_or_hhx4);
  RUN_TEST(test_atat_sets_adaptive_timing_mode);
  RUN_TEST(test_atctm_sets_timeout_multiplier);
  RUN_TEST(test_atpc_disconnects_and_leaves_monitor_mode);
  RUN_TEST(test_atsh_sets_standard_and_extended_header_and_rejects_out_of_range);
  RUN_TEST(test_atcp_sets_priority_bits);
  RUN_TEST(test_atcra_and_atar_clear_receive_address_and_filter);
  RUN_TEST(test_atcra_sets_exact_receive_address);
  RUN_TEST(test_atcf_and_atcm_set_filter_and_mask_independently);
  RUN_TEST(test_atcaf_and_atcfc_toggles);
  RUN_TEST(test_atfcsm_sets_flow_control_mode);
  RUN_TEST(test_atfcsh_sets_manual_flow_control_id);
  RUN_TEST(test_atfcsd_sets_manual_flow_control_bytes);
  RUN_TEST(test_atcea_atceahh_atcerhh);
  RUN_TEST(test_atma_atmr_atmt_enter_monitor_mode_silently);
  RUN_TEST(test_atcs_reports_typed_status);
  RUN_TEST(test_atcsm_toggles_silent_monitoring);
  RUN_TEST(test_atrtr_sends_remote_frame_with_current_header);
  RUN_TEST(test_atv_toggles_variable_dlc_flag);
  RUN_TEST(test_atbd_reports_no_data_then_the_last_accepted_frame);
  RUN_TEST(test_monitor_all_prints_every_frame);
  RUN_TEST(test_monitor_received_address_filters_by_low_byte);
  RUN_TEST(test_unsupported_commands_return_unknown_without_side_effects);
  return UNITY_END();
}
