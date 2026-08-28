# T10 - Compatibility Validation and Release Gate

**Status:** Planned

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
