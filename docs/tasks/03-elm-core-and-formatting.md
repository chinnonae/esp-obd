# T03 - ELM Core and Reply Formatting

**Status:** Planned

## Goal

Implement a portable ELM command engine that transforms a complete input line
into a structured action and formatted reply without writing to a stream or
touching a CAN controller.

## Scope

- `ElmSession` defaults and session-only state.
- Input normalization, empty-command repeat, AT-vs-hex classification, and
  strict validation.
- Core commands: reset, identity, echo, linefeeds, spaces, headers, DLC, and
  responses. Protocol description (`ATDP`/`ATDPN`) is out of scope here; it
  depends on protocol-selection state owned by
  [T09](09-can-commands-and-monitoring.md).
- A reply formatter for CR/LF, prompts, byte formatting, and errors.

## Steps

1. Define `ElmReply` and action/result types that can represent text, a pending
   diagnostic request, monitor start/stop, and an error.
2. Port parser behavior from the command contract one family at a time.
3. Keep command handlers pure: they may change a session or return an action,
   but cannot call `ICanPort`.
4. Put all response spelling in one formatter; do not scatter `"OK"` and `"?"`.
5. Create contract tests for every implemented row before adding the next row.

## Acceptance criteria

- Core command tests run with no ESP32 or fake CAN dependency.
- Invalid commands leave a byte-for-byte session copy unchanged.
- Formatting of `ATH`, `ATS`, `ATL`, and `ATD` combinations matches the command
  contract exactly.
- The configured ELM identity is tested from one shared constant.

## Tests

- `ATZ`, `ATD`, `ATI`, and empty-command repeat.
- Malformed AT commands and invalid hexadecimal input.
- Complete formatting matrix for a known CAN frame.
