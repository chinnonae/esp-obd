# T02 - Portable CAN Core

**Status:** Done (started 2026-08-28, completed 2026-08-28)

## Goal

Create the small, hardware-independent vocabulary shared by ELM, ISO-TP, and
the ESP32 TWAI adapter.

## Scope

- `CanFrame`, identifier validation, DLC validation, and remote-request flag.
- `CanConfig`, bitrate enum, controller mode, status, and typed send/receive
  results.
- `ICanPort` and `IClock` interfaces.
- OBD default addresses and pure acceptance/filter matching functions.

## Steps

1. Put value types in `include/can/` and implementations in `src/can/`.
2. Use `std::array<uint8_t, 8>` and fixed-width integers; do not use Arduino
   `String`, `Stream`, or heap allocation.
3. Add factory helpers that reject invalid 11-bit/29-bit IDs and DLC values.
4. Implement address/filter logic as pure functions.
5. Update fake CAN types from T01 to implement `ICanPort`.

## Acceptance criteria

- Standard, extended, and RTR frames are represented without TWAI types.
- Invalid IDs and DLCs cannot reach an `ICanPort` send call.
- Filter matching has complete tests for masks, exact response IDs, and default
  11-bit/29-bit OBD response ranges.

## Tests

- Frame creation boundary values: `7FF`, `800`, `1FFFFFFF`, `20000000`.
- DLC boundary values: 0, 8, and 9.
- Software filter truth table for mask/filter/receive-address combinations.

## Notes

- Delivered under `include/can/`: `can_frame.h` (`CanFrame`, id/dlc
  validation, `makeStandardFrame`/`makeExtendedFrame`/`make*RemoteFrame`),
  `can_config.h` (`Bitrate`, `ControllerMode`, `CanConfig`), `i_clock.h`
  (`Milliseconds`, `IClock`), `can_result.h` (`CanResult`, `ReceiveResult`,
  `CanStatus`), `i_can_port.h` (`ICanPort`), `can_filter.h` (`CanFilter`,
  `matchesFilter`, `exactIdFilter`), `obd_addresses.h` (default 11-/29-bit
  request/response addresses). Implementation in `src/can/can_frame.cpp`.
- `ICanPort::receive()` takes no timeout and never blocks, per the contract
  decided in [ARCHITECTURE.md](../ARCHITECTURE.md).
- `test/support/fake_can_port.h` now implements `ICanPort` directly (no more
  ad hoc `FakeCanFrame`), and `test/support/can_frame_builder.h` builds real
  `CanFrame`s via the factories, asserting on an invalid result since the
  builder is for valid test fixtures only.
- `platformio.ini`'s `native_test` env now has `test_build_src = true` with
  `build_src_filter = -<*> +<can/>`; later layers (isotp/, diagnostic/,
  elm/, app/) extend this filter as their tasks land. `src/main.cpp` and
  `platform/esp32/` must never be added to it.
- New suite `test/unit/test_can_core/`: frame id/dlc boundaries, remote
  frames, filter truth table (mask/value, exact-id, default OBD ranges),
  and `FakeCanPort` tx-order/non-blocking-rx behavior.

Verified: `pio test -e native_test` (13/13 passing across `test_smoke` and
`test_can_core`), and `pio run -e ioxesp32` still builds with `can/` in the
firmware image.
