# T01 - Native Test Foundation

**Status:** Planned

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
