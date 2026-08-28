# T08 - Bluetooth ELM and UART Debug

**Status:** Planned

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
