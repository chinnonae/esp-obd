# T10 - Compatibility Validation and Release Gate

**Status:** Blocked (started 2026-08-28) -- everything checkable without a
physical board, scanner app, or vehicle is done; the rest genuinely needs
them. See **Blocker** below.

## Goal

Prove that the new adapter is dependable on a simulator, target hardware, and
the intended Bluetooth scanner applications before calling it usable.

## Scope

- Native regression suite and a documented CI command.
- ISO-TP/CAN simulator scenarios for standard OBD, VIN, physical addressing,
  no-data, and errors.
- Bluetooth initialization traces from the selected scanner apps.
- Bench and stationary-vehicle smoke checks with a rollback plan.
- Final documentation and known-limitations review.

## Steps

1. Freeze a list of target apps and collect their initialization traces.
2. Replay those traces against native integration tests.
3. Create a bench checklist with CAN termination, bitrate, receive, transmit,
   multi-frame, and monitoring checks.
4. Run a stationary-vehicle checklist with read-only requests first.
5. Record results, firmware version, board revision, app version, and failures.
6. Update README, behavior contract, and task index before tagging a release.

## Acceptance criteria

- Native tests and target firmware build are green from a clean checkout.
- Every target app completes its initialization handshake without protocol
  formatting errors.
- Bench and vehicle tests cover 11-bit and 29-bit traffic where available.
- Known unsupported hardware features are visible in the final documentation.
- No safety-critical or compatibility blocker remains open in the index.

## Tests and evidence

- Save anonymized app traces and simulator scripts under a future
  `test/fixtures/` directory.
- Attach a dated bench/vehicle checklist result to this task before completion.

## Blocker

This session has no physical ESP32 board, CAN bench, scanner-app phone, or
vehicle, and no internet access to source real app initialization traces
without fabricating them. The genuinely hardware/field-dependent parts of
this task's scope are not done and must not be claimed as done:

- Step 1-2 (freeze a target-app list, collect and replay their real
  initialization traces) -- needs the actual apps.
- Step 3-4 (bench and stationary-vehicle checklists) -- needs the board, a
  CAN bench/simulator harness, and a vehicle.
- Step 5 (record firmware/board/app versions and results) -- nothing to
  record yet.
- The acceptance criteria about scanner-app handshakes and bench/vehicle
  11-bit/29-bit coverage.

Do not mark this task `[x]` until someone runs that checklist on real
hardware; record the dated result here when that happens.

## Notes (what was actually done this session)

- **"Native tests and target firmware build are green from a clean
  checkout"** (acceptance criterion) -- verified directly: deleted `.pio`
  entirely and rebuilt from scratch. `pio test -e native_test` passed
  155/155 across fourteen suites, and `pio run -e ioxesp32` built the full
  firmware clean, both with no prior build cache.
- **"Native regression suite and a documented CI command"** -- already
  satisfied by T01/T08's work: `pio test -e native_test` is documented in
  the top-level [README.md](../../README.md) and
  [test/README.md](../../test/README.md).
- **"ISO-TP/CAN simulator scenarios for standard OBD, VIN, physical
  addressing, no-data, and errors"** -- already covered by existing native
  suites against the `FakeCanPort` simulator: `test_diagnostic_transport`
  (physical/functional, no-data, bus error, auto-search), `test_isotp_
  receive`/`test_isotp_transmit` (multi-frame, timeouts, flow control). A
  VIN-shaped (mode `09`) multi-frame scenario specifically wasn't added as
  its own named test; the mode/PID bytes don't matter to the ISO-TP/
  diagnostic layers, which are already exercised with arbitrary multi-byte
  payloads, so this is a documentation gap rather than a coverage gap.
- **"Known unsupported hardware features are visible in the final
  documentation"** -- confirmed still accurate in the README's hardware
  section (no J1850/ISO-KWP/voltage/ignition/CAN FD claims).
- Reconciled [ELM_COMMAND_BEHAVIOR.md](../ELM_COMMAND_BEHAVIOR.md)'s
  closing section, which still described the *pre-reimplementation*
  legacy gaps (faked `ATRV`/`ATIGN`, no multi-frame TX) -- replaced with
  the actual current gaps (manual FC injection, `ATCP` priority bits,
  variable DLC wire format, `ATBD`'s TX side), pointing at the owning task
  files instead of duplicating detail.
- Updated the top-level [README.md](../../README.md) to describe the real
  firmware (Bluetooth ELM327 channel + UART debug console) instead of the
  T00 hello-world placeholder, while being explicit that it's unverified
  on hardware.
- "No safety-critical or compatibility blocker remains open in the index"
  -- not satisfied: T07 and this task are both `[!]` for the same
  underlying reason (no hardware), which is an open item by definition,
  not a hidden one.
