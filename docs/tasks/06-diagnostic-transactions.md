# T06 - Diagnostic Transactions

**Status:** Done (started 2026-08-28, completed 2026-08-28)

## Goal

Combine CAN addressing and ISO-TP state machines into a deterministic service
that executes one ELM diagnostic request and returns structured responders.

## Scope

- Lives in its own `diagnostic/` layer (see [Architecture](../ARCHITECTURE.md)),
  between `isotp/` and `elm/`: depends on `can/` and `isotp/`, and must not
  depend on `elm/`.
- Functional versus physical request behavior.
- Multiple responder collection, response limits, filters, and receive address.
- CAN-only automatic search order `6 -> 7 -> 8 -> 9`.
- Mapping transport results to ELM-level outcomes without formatting text here.

## Steps

1. Define a `DiagnosticRequest` from session state and parsed input.
2. Route it through ISO-TP TX then create a receive state per accepted source.
3. Enforce the responder cap and requested response count.
4. Implement auto-search as an explicit sequence of configurations, recording
   the successful protocol only after a complete response.
5. Return responders, raw frames, and typed failure details to the ELM layer.

## Acceptance criteria

- A functional request can return two distinct source responses.
- A physical request stops at its first complete matching response.
- No-data, bus error, and auto-search exhaustion are distinct results.
- The service does not format ELM text or depend on Bluetooth.

## Tests

- One/two responder scenarios, filtering, and explicit response count.
- 11-bit and 29-bit auto-search success and exhaustion.
- Receive timeout after TX and a transmit failure before RX.

## Notes

- `can::obd::ObdCanProtocol` (4 wire configurations, no "automatic" member)
  plus `bitrateFor`/`isExtendedCan`/`functionalRequestId`/
  `isDefaultObdResponse`/`computeFlowControlId`/`kAutoSearchOrder` were
  added to `include/can/obd_addresses.h`, not created fresh in
  `diagnostic/`, since `diagnostic/` must not depend on `elm/` and couldn't
  reuse `elm::ElmProtocol` anyway. `elm::ElmProtocol` (T03) still has its
  own 5-value enum (adds "AutomaticSearch" as session state); T09, which
  actually implements `ATSP` and will feel any friction directly, decides
  whether to refactor `ElmSession.protocol` to wrap this type instead.
- `DiagnosticTransport` is itself `start()`/`poll(now)`-driven, not a
  blocking call: a request can span many poll ticks (multi-frame TX/RX,
  auto-search across up to 4 protocol attempts), consistent with every
  layer under it. It is the first layer to call `ICanPort::receive()`
  directly (isotp/ only ever had frames handed to it).
- Auto-search stops at the *first* confirming response (not up to
  `kMaxResponders`) to lock a protocol quickly; a full multi-ECU functional
  collection happens on a later request made through `start()` once already
  connected, not during the search itself.
- Deliberate simplifications, documented here rather than the header: (1)
  a bus/driver error transmitting the *request* aborts the whole
  transaction (`BusError`), but one responder's own send failure (e.g. its
  Flow Control) just drops that responder rather than failing everything;
  (2) a responder still mid-reassembly when the overall deadline elapses is
  excluded, not separately reported; (3) an incoming frame during
  multi-frame request transmission is routed to the transmitter's
  `onFlowControl` unconditionally (it already validates PCI and ignores a
  mismatched state) rather than address-matched first, since a functional
  request is realistically always short enough to need only a Single
  Frame.
- New suite `test/unit/test_diagnostic_transport/`: physical stop-at-first
  vs. functional collect-two, an explicit response count stopping early,
  `ATCRA`-style filtering, no-data, a transmit-side bus error, and
  auto-search succeeding on the second (29-bit) candidate after the first
  is silent, plus full 4-candidate exhaustion.

Verified: `pio test -e native_test` (84/84 passing across eight suites) and
`pio run -e ioxesp32` still builds.
