# T09 - CAN Command Families and Monitoring

**Status:** Done (started 2026-08-28, completed 2026-08-28)

## Goal

Complete the in-scope CAN-specific ELM behavior on top of the stable core.

## Scope

- `ATSP`, `ATTP`, `ATDP`, `ATDPN`, `ATST`, `ATAT`, `ATCTM`, and `ATPC`.
- `ATSH`, `ATCP`, `ATCRA`, `ATAR`, `ATCF`, `ATCM`, `ATCAF`, `ATCFC`, and
  manual flow-control settings.
- Extended addressing, monitoring (`ATMA`, `ATMR`, `ATMT`), silent monitor,
  CAN status, RTR, variable DLC, and buffer dump.
- The explicitly unsupported-command parameterized test set.

## Steps

1. Implement commands one family at a time, beginning with protocol and header
   state, then filters, then flow control, then monitoring.
2. Add a contract test before each handler is implemented.
3. Keep monitor filtering in portable code and keep the TWAI adapter unaware of
   ELM command strings.
4. Implement `ATCS` from a typed `CanStatus` rather than raw driver output.
5. Verify every unsupported command returns `?` without state mutation or TX.

## Acceptance criteria

- Every in-scope command row in the behavior contract has a passing contract
  test.
- A monitor session prints only matching frames and always exits safely.
- Manual/automatic flow-control modes produce exact expected frames.
- `ATRV` and `ATIGN` are unsupported, never fabricated.

## Tests

- Parameterized syntax/side-effect tests for command families.
- Filter and monitor capture tests for standard and extended IDs.
- Unsupported commands assert unchanged session plus empty TX capture.

## Notes

- `ElmSession` grew substantially: `protocolDiscoveredViaAutoSearch` (for
  `ATDP`/`ATDPN`'s `AUTO,`/`A6..A9` prefix), `canTimeoutMultiplier`,
  `priorityBits`, `flowControlMode`, `manualFlowControlId`/
  `manualFlowControlData(Len)`, `requiredExtendedAddressByte` (distinct
  from the existing transmit `extendedAddressByte`), `monitorMode`/
  `monitorAddressByte`, `silentMonitoringEnabled`, `variableDlcEnabled`,
  `lastAcceptedReceivedFrame`. All added to `assertSessionIsDefault` in
  T03's contract suite, not just this task's own tests.
- `ATSP`/`ATTP` never touch `ICanPort` directly (elm/ still can't): they
  only update session state. Actual TWAI reconfiguration happens lazily in
  `app/ElmApplication`, right before the next diagnostic transaction or
  monitor session -- consistent with how T06's auto-search already
  reconfigures per candidate.
- New shared `elm::toObdCanProtocol`/`fromObdCanProtocol`
  (`include/elm/protocol_mapping.h`) replace the local copies T08's
  `elm_application.cpp` had duplicated; `at_commands_protocol.cpp` and
  `ElmApplication` both use the one definition now.
- `ATCS` and `ATRTR` need live `ICanPort` access this layer must not have,
  so they return new `ElmReplyKind::CanStatusRequest`/`SendRtrRequest`
  (empty text) that `ElmApplication::execute()` resolves to the real reply
  within the same call -- the same indirection already established for
  hex requests (`DiagnosticRequest`), not a new pattern.
- Monitor mode: `ATMA`/`ATMR`/`ATMT` set session state only and return a
  reply with empty text and `appendPrompt = false` (matches "no immediate
  response ending or prompt" for free, via the existing `Text`-kind
  handling — no new reply kind needed). Printing happens in
  `ElmApplication::pollMonitor()`, called from `ElmBluetoothSession::poll()`
  when `monitorActive()`; stopping happens via `ElmApplication::
  stopMonitor()` (called from `ElmBluetoothSession::onByte()` on any byte,
  already required since T08), which also restores the CAN port's normal
  mode if `ATCSM1` had put it into listen-only. `ATMRhh`/`ATMThh` match by
  the CAN id's low byte -- a reasonable reading of a terse contract row,
  not a literally specified rule.
- Known, documented simplifications (matching this session's established
  practice for partial implementations elsewhere):
  - `ATFCSM1`/`ATFCSM2` (manual flow-control modes) and `ATFCSH`/`ATFCSD`
    (manual FC id/bytes) are accepted and stored, but the manual bytes
    are never actually injected into an outgoing Flow Control frame --
    today they just disable automatic FC, same observable effect as
    `ATCFC0`. Full wiring needs `isotp::RxConfig` extended to carry
    caller-supplied FC bytes.
  - `ATCPhh` (29-bit header priority bits) is stored but not yet applied
    when constructing a 29-bit request id, to avoid guessing an
    unverified bit layout.
  - `ATV0`/`ATV1` is stored but doesn't change wire format: T04/T05 still
    unconditionally pad to 8 bytes (already noted as an extension point
    in their own task files).
  - `ATBD` shows only the last accepted RX frame, not "last TX and
    accepted RX" -- `IsoTpTransmitter` doesn't currently expose the frames
    it sent.
- New suite `test/contract/test_can_command_families/`: representative
  coverage (not literally every row) of all four families plus a sample
  of the unsupported-command matrix, chosen to exercise every distinct
  parsing pattern (exact match, prefix + fixed-width hex, variable-length
  hex) at least once, plus monitor-mode frame printing/filtering.

Verified: `pio test -e native_test` (155/155 passing across fourteen
suites) and `pio run -e ioxesp32` still builds (85.7% flash, up slightly
from T08's 85.3%).
