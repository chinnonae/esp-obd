# Native Tests

Fast, hardware-free tests for the portable code (`can/`, `isotp/`,
`diagnostic/`, `elm/`, `app/`). These run entirely on the desktop: no ESP32
board, TWAI driver, or Bluetooth stack involved.

## Running

Whole suite:

```
pio test -e native_test
```

One suite (`-f` filter matches a suite's path under `test/`):

```
pio test -e native_test -f "unit/test_smoke"
```

## Layout

- `unit/` - one component in isolation: parser, formatter, address/filter
  matching, ISO-TP transitions. Fakes allowed: clock, CAN port.
- `contract/` - one test per row of
  [ELM_COMMAND_BEHAVIOR.md](../docs/ELM_COMMAND_BEHAVIOR.md). Fakes allowed:
  diagnostic transport.
- `integration/` - command -> ISO-TP -> CAN frames -> reply, end to end.
  Fakes allowed: fake CAN bus + clock.

`contract/` and `integration/` are created by the tasks that add their first
suite (T03 onward); they don't exist yet. See
[docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md)'s "Testing strategy" section
for the reasoning behind this split.

## Shared test support

`support/` holds reusable fakes and builders used across suites. Headers
only, no test suite of its own:

- `fake_clock.h` - `FakeClock`, a monotonic clock that only advances when
  explicitly told to.
- `fake_can_port.h` - `FakeCanPort` and `FakeCanFrame`: captures transmitted
  frames in order and serves queued receive frames without blocking.
- `can_frame_builder.h` - `CanFrameBuilder` for readable test frame
  construction.
- `elm_reply_assertions.h` - `expectReply(actual).toEqual(expected)` for
  readable ELM text assertions.

`FakeCanFrame` is a deliberate test-only stand-in for the `CanFrame` type
[T02](../docs/tasks/02-portable-can-core.md) adds under `can/`. Once that
lands, `fake_can_port.h` should be updated to use `CanFrame` directly and to
implement `ICanPort`, per T02's own task file -- not redesigned from scratch.
