# T08 - Bluetooth ELM and UART Debug

**Status:** Done (started 2026-08-28, completed 2026-08-28)

## Goal

Expose a clean ELM327-style Bluetooth SPP channel while retaining a useful,
strictly separate UART0 `#DBG:` development console.

## Scope

- Reusable bounded `LineReader` events: line, overflow, timeout, disconnect.
- Bluetooth connection lifecycle, echo, replies, prompts, and monitor stop.
- UART debug commands: `#HELP`, `#STATUS`, `#DBG 0..3`, and `#REBOOT`.
- A debug sink that prefixes every UART line and can never target Bluetooth.

## Steps

1. Implement and unit-test `LineReader` as portable code.
2. Implement Bluetooth adapter code that passes completed lines to
   `ElmApplication` and writes only its replies.
3. Implement the UART console using a separate parser and output sink.
4. Handle connection/disconnection by resetting only Bluetooth transport state
   according to the documented policy.
5. Wire monitor mode so any Bluetooth byte stops it and is discarded.

## Acceptance criteria

- Bluetooth receives no `#DBG:` output at any debug level.
- Echo, prompt, line ending, overflow, timeout, and monitor-stop behavior match
  the command contract.
- UART commands cannot be parsed as ELM commands.
- Debug logs remain useful when Bluetooth is disconnected.

## Tests

- Line-reader boundary and timeout cases.
- Captured Bluetooth conversation for `ATZ`, `ATE0`, and `0100`.
- Assertion that debug activity changes only the fake UART sink.

## Notes

- All four acceptance criteria are behavioral and fully verified in
  `native_test` against portable code, not held back by the lack of
  hardware: `Esp32UartDebugSink` has no reference to any Bluetooth type at
  all (so "Bluetooth receives no `#DBG:` output" and "debug logs remain
  useful when Bluetooth is disconnected" hold structurally, not just by
  test coverage), and `parseDebugCommand` is a separate, case-sensitive
  literal matcher unrelated to the ELM parser (so a UART command
  structurally cannot be parsed as an ELM one).
- `include/app/line_reader.h`: `LineReader` is reusable, portable, and
  `poll(now)`-driven (never blocks). Overflow is reported immediately when
  a byte would exceed capacity (discarding buffer and byte together) rather
  than waiting for an eventual `CR` that might never come; the contract
  doesn't pin exact overflow/timeout reply text, so `ElmBluetoothSession`
  responds to Overflow with `?` (treating an unterminated too-long line as
  malformed) and to a mid-line idle timeout with silence -- both
  interpretations, not directly specified rows.
- `include/app/elm_bluetooth_session.h`: the portable Bluetooth
  conversation logic (echo, line assembly, prompt) that a captured-
  conversation test can exercise without any real Bluetooth stack. This is
  also where the `DiagnosticRequest`-kind `ElmReply` finally gets turned
  into a real reply -- `ElmApplication` gained `execute(now, line)`,
  `poll(now)`, `diagnosticPending()`, and `takeDiagnosticReply()` to
  support it (a Single Frame request/response typically completes
  synchronously within `execute()`; anything needing more ticks is picked
  up by the transport's own `poll(now)`). `formatDiagnosticResult()`
  renders one line per responder in `CAF1` mode (forcing headers on when
  more than one answers, per contract section 1.4), and in `ATH1` mode one
  line per *raw frame* (First Frame + each Consecutive Frame), each
  trimmed for CAN-level padding under `ATD0` -- originally shipped
  showing only each responder's first raw frame, which real-hardware
  testing against a scanner app caught truncating a multi-frame VIN read
  to ~3-6 characters; fixed post-T09, see
  [T07](07-esp32-platform-adapters.md)'s Hardware validation update.
  A successful auto-search is persisted into
  `session.protocol`/`protocolConnected` here (needed so a
  second request doesn't re-search every time); T09's `ATDPN` will need to
  separately track "discovered via search" for its `A6..A9` vs `6..9`
  distinction, not modeled yet.
- `include/app/debug_console.h`: portable `#HELP`/`#STATUS`/`#DBG 0-3`/
  `#REBOOT` parsing plus the `#DBG: ` line prefix, both pure/testable.
- ESP32 glue (`Esp32BluetoothTransport`, `Esp32UartDebugSink`) is thin by
  design -- it only shuttles real `BluetoothSerial`/`Serial` bytes through
  the portable classes above and writes back whatever they return. Like
  T07's adapters, it has no native test (no ESP-IDF/Bluetooth headers on
  the host) and is verified here only by `pio run -e ioxesp32`; the
  `#REBOOT` handler's `delay(50)` before `ESP.restart()` is a deliberate,
  one-shot exception to "never `delay()`" (an imminent hard reset, not part
  of the ongoing loop).
- `main.cpp` now wires the real adapters end to end (replacing the
  hello-world blink from T00): constructs `Esp32TwaiCanPort`,
  `Esp32Clock`, `Esp32SettingsStore`, `ElmApplication`,
  `Esp32BluetoothTransport`, and `Esp32UartDebugSink`, configures the CAN
  port at 500 kbit/s, and polls both transports from `loop()`. The status
  LED blink was dropped as no longer relevant scope, not preserved.
- Flash usage jumped from ~20% to 85.3% of the 1.3MB app partition once
  `BluetoothSerial` (the Bluedroid classic-BT stack) linked in. Still
  builds fine, but there's only ~15% headroom left for T09's additional
  CAN commands -- worth watching, and a candidate flag for T10 if it gets
  tight.

Verified: `pio test -e native_test` (126/126 passing across thirteen
suites) and `pio run -e ioxesp32` builds clean with the fully-wired
`main.cpp`.
