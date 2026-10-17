# T00 - Project Baseline and Decisions

**Status:** Done (started 2026-08-28, completed 2026-08-28)

## Goal

Turn the clean hello-world repository into an agreed starting point without
pulling code from the legacy implementation by accident.

## Scope

- Confirm the target board, CAN TX/RX pins, LED polarity, Bluetooth name, and
  pairing policy.
- Establish the supported hardware boundary: Classical CAN only, no battery or
  ignition measurement, and no non-CAN OBD electrical protocols.
- Write/control request policy: decided. No whitelist — the transport
  transmits any well-formed diagnostic payload without inspecting or gating
  the service ID, matching real ELM327 behavior and
  [ELM_COMMAND_BEHAVIOR.md](../ELM_COMMAND_BEHAVIOR.md) §2.5. There is no
  debug-only unlock step for mutating services (e.g. UDS write/routine/reset
  requests); a caller with Bluetooth access can send them exactly as a real
  ELM327 adapter would.
- Reconcile the behavior contract with the clean start: remove references to
  deleted implementation details and mark all future behavior as target work.
- Add a small project version and an agreed ELM identity string.

## Steps

1. Create `include/core/build_info.h` with firmware version and identity.
2. Create a short hardware constants file with only confirmed pin assignments.
3. Add a concise safety policy to the README and behavior contract.
4. Record each non-obvious decision in this task file under **Decision log**.
5. Add an initial build check to the documented workflow.

## Acceptance criteria

- A new contributor can tell what hardware is and is not supported from the
  README without reading source code.
- No document promises voltage, ignition, J1850, ISO/KWP, or CAN FD support.
- The ELM identity used by code and tests has one source of truth.
- The write/control policy is explicit, testable, and approved.

## Tests

- Build the hello-world target with PlatformIO.
- Add a compile-time test or static assertion for the selected hardware values.

## Decision log

Add dated entries here. Do not overwrite old decisions; supersede them.

- **2026-08-28 — Write/control request policy.** No whitelist: the adapter
  does not gate or require an unlock for mutating diagnostic services (UDS
  write/routine/reset requests, etc.). Any well-formed payload transmits
  exactly as on a real ELM327. This matches the behavior already specified in
  [ELM_COMMAND_BEHAVIOR.md](../ELM_COMMAND_BEHAVIOR.md) §2.5.
- **2026-08-28 — Board, CAN pins, LED.** Target board is `ioxesp32`
  (confirmed against the installed board variant's `pins_arduino.h`, which
  also fixes `LED_BUILTIN` at GPIO 5 — matching the existing hello-world
  blink). CAN TX/RX are GPIO 26/27, reusing the wiring from the legacy
  adapter (`f5c36ed:include/config.h`) rather than re-deriving it. Recorded
  in [include/core/hardware_constants.h](../../include/core/hardware_constants.h).
- **2026-08-28 — Bluetooth identity.** Device name `ESP-OBD`, fixed pairing
  PIN `5678`, reusing the legacy adapter's values so previously paired
  scanner apps do not need to re-pair. Recorded in
  [include/core/hardware_constants.h](../../include/core/hardware_constants.h).
- **2026-08-28 — Firmware version.** Restarted at `0.1.0`: this is a
  from-scratch reimplementation, and the legacy `0.1.0` release is retired
  along with the rest of the legacy code. Recorded in
  [include/core/build_info.h](../../include/core/build_info.h).
- **2026-08-28 — Build standard.** Set `-std=gnu++17` in `platformio.ini`
  (overriding the framework's default `-std=gnu++11`) so header-only
  `inline constexpr` constants in `include/core/` don't trigger per-TU
  duplicate-definition risk or a compiler warning.
