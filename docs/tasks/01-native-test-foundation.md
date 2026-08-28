# T01 - Native Test Foundation

**Status:** Done (started 2026-08-28, completed 2026-08-28)

## Goal

Make fast desktop tests the default way to develop portable protocol logic.

## Scope

- Add a `native_test` PlatformIO environment that compiles only portable code.
- Establish test directory conventions: `unit/`, `contract/`, and
  `integration/`.
- Provide test helpers: fake monotonic clock, fake CAN port, CAN frame builder,
  and readable ELM reply assertions.
- Make test failure output useful enough to diagnose a mismatched CAN byte or
  response string without a debugger.

## Steps

1. Add the native environment without Arduino, TWAI, or Bluetooth headers.
2. Add one smoke test that proves the test runner executes.
3. Implement `FakeClock` with explicit time advancement only.
4. Implement `FakeCanPort` with separate TX capture and queued RX frames.
5. Document one command to run the whole native suite and one to run a test
   group.

## Acceptance criteria

- `pio test -e native_test` completes on a development machine without ESP32
  hardware.
- A unit test can inspect exact transmitted frames and advance time without
  sleeping.
- No production portable header requires an Arduino or ESP-IDF include.

## Tests

- Fake clock advances only when instructed.
- Fake CAN port preserves frame order and records timeouts.
- An intentionally failing assertion displays expected and actual bytes.

## Notes

- No separate host toolchain install was needed or added: `native_test` uses
  the `zig cc`/`c++`/`ar` drivers already bundled via the `ziglang` pip
  package in `.venv`, wired in through `test/native_toolchain.py` and thin
  `test/native_tools/*.cmd` wrappers (the native platform's own builder
  deletes any `CC`/`CXX` set directly and re-detects `gcc`/`g++` from `PATH`,
  so the wrappers are what it finds). See [test/README.md](../../test/README.md).
- `FakeCanPort::receive()` takes no timeout and never blocks, matching the
  `ICanPort` contract decided in [ARCHITECTURE.md](../ARCHITECTURE.md): it
  either returns a queued frame or reports none immediately. A caller
  wanting timeout behavior computes its own deadline from `FakeClock` and
  polls; the port itself has nothing to record.
- `FakeCanFrame` intentionally duplicates the `CanFrame` shape from
  `ARCHITECTURE.md` rather than defining it once here — T02 owns that type
  and will point `fake_can_port.h` at it directly.

Verified: `pio test -e native_test` (5/5 passing), `pio test -e native_test
-f "unit/test_smoke"` (group filter), and `pio run -e ioxesp32` (ESP32
target unaffected) all pass. An intentional wrong assertion was confirmed to
print `Expected 6 Was 8` before being fixed.
