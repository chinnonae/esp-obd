# T02 - Portable CAN Core

**Status:** Planned

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
