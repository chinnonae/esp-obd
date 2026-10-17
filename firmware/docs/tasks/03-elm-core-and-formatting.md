# T03 - ELM Core and Reply Formatting

**Status:** Done (started 2026-08-28, completed 2026-08-28)

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

## Notes

- Added AT@1 (fixed adapter description) to this task's scope: it needs no
  persisted state, unlike AT@2/AT@3, so it belongs with ATI as a stateless
  identity command rather than with T11.
- `ElmSession` ([include/elm/elm_session.h](../../include/elm/elm_session.h))
  models every row of the section 1.3 defaults table now (not just the
  fields T03's own handlers mutate), so `ATZ`'s "assert every default in
  one test" (contract section 4, item 1) can be written today. Fields owned
  by later tasks (protocol, addressing, monitor, ...) just sit at their
  documented default; T09/T11 add handlers, not new fields.
- `ElmReply` gained a `DiagnosticRequest` kind beyond the three
  ([Text/StartMonitor/StopMonitor/NoReply](../ARCHITECTURE.md)) originally
  sketched, to carry a validated hex request through to T06. Byte-decoding
  is deliberately deferred to T06: T03 only validates syntax and returns the
  normalized payload hex text, so decoding isn't duplicated across layers.
- `elm_formatter`'s header/DLC rendering takes an explicit `rawBytesToShow`
  rather than recomputing it: the worked example in
  [ELM_COMMAND_BEHAVIOR.md](../ELM_COMMAND_BEHAVIOR.md) section 1.4 shows
  `ATD0` trimming CAN-level padding even with headers on, which requires
  ISO-TP PCI knowledge T04 owns, not the formatter.
- New `core/fixed_string.h`: a small fixed-capacity, no-heap string
  (matching `ElmReply`'s documented `FixedString<...>` in
  [ARCHITECTURE.md](../ARCHITECTURE.md)), used for both parsed input lines
  and reply text.
- `native_test`'s `build_src_filter` now includes `elm/` alongside `can/`.
- New suites: `test/unit/test_elm_parser/`, `test/unit/test_elm_formatter/`,
  `test/contract/test_core_session_commands/` (first use of `contract/`).

Verified: `pio test -e native_test` (46/46 passing across all five suites),
and `pio run -e ioxesp32` still builds with `elm/` in the firmware image.
