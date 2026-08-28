# T04 - ISO-TP Receive State Machine

**Status:** Done (started 2026-08-28, completed 2026-08-28)

## Goal

Reliably collect a single responder's ISO-TP response, including automatic or
manual flow control, without blocking the rest of the application indefinitely.

## Scope

- Single Frame, First Frame, Consecutive Frame, and Flow Control parsing.
- Declared-length tracking up to the chosen bounded payload limit.
- Sequence validation, receive timeouts, extended addressing, and padding.
- Header-on raw-frame reporting data in addition to reassembled payloads.

## Steps

1. Define explicit receive states and typed terminal results.
2. Implement `start()` and `poll(now)`; neither function calls `delay()`.
3. On a valid First Frame, choose automatic/manual/no flow control according to
   session settings and queue the correct frame.
4. Reject invalid PCI, wrong sequence, overflow, and mismatched extended
   address without returning a successful partial payload.
5. Expose raw frames and completed payload separately for the ELM formatter.

## Acceptance criteria

- A known VIN-like response is reassembled exactly.
- The timeout behavior is deterministic under `FakeClock`.
- Flow-control ID/data behavior is correct for both 11-bit and 29-bit OBD.
- No receive path reads beyond the configured fixed buffer.

## Tests

- Single-frame payload and padding removal.
- Multi-frame happy path, sequence wrap, missing CF, wrong sequence, and
  declared length larger than the limit.
- Headers-on output data and extended-address matching.

## Notes

- New `include/isotp/isotp_pci.h` + `src/isotp/isotp_pci.cpp`: shared
  Single/First/Consecutive/Flow-Control PCI parsing and FC-byte building,
  used by both this task and (later) T05's transmit side.
- `IsoTpReceiver` ([include/isotp/isotp_receive.h](../../include/isotp/isotp_receive.h))
  holds a bounded `kMaxPayloadBytes = 255` payload buffer and
  `kMaxRawFrames = 40` raw-frame buffer -- chosen to comfortably cover real
  OBD-II/UDS responses while keeping "declared length larger than the
  limit" actually reachable and testable (the ISO-TP theoretical max is
  4095). `receive()`-style access is via `poll(now)`/`onFrame`, never a
  blocking call; timeouts are computed from the caller-supplied `now`, per
  the `ICanPort` contract in [ARCHITECTURE.md](../ARCHITECTURE.md).
- Per `ELM_COMMAND_BEHAVIOR.md`'s `ATCERhh` row, an extended-address
  mismatch is silently ignored (state stays `Idle`/unchanged), not treated
  as a protocol error.
- `RxConfig` only implements the simple `ATCFC0`/`ATCFC1` automatic-or-not
  toggle. Full `ATFCSM0`/`ATFCSM1`/`ATFCSM2` manual-header/manual-data modes
  are T09's job and will extend `RxConfig`, not replace it.
- Flow Control frames are always padded to a full 8-byte classical CAN
  frame; T09's `ATV0`/`ATV1` may need to make this configurable.
- New suite `test/unit/test_isotp_receive/`: single-frame, multi-frame happy
  path, sequence wrap 15->0, missing/wrong sequence, oversized declared
  length, timeout waiting-for-first-CF and mid-stream, extended-address
  match/mismatch (including on the FC frame itself), FC id/data for both
  11-bit and 29-bit OBD, FC re-arm at a configured block size, and a
  maximum-length transfer to confirm the raw-frame buffer never overflows.

Verified: `pio test -e native_test` (61/61 passing across six suites) and
`pio run -e ioxesp32` still builds with `isotp/` in the firmware image.
