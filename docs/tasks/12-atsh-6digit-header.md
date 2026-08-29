# T12 - ATSH 6-Hex-Digit Header Form

**Status:** Done (started 2026-08-29, completed 2026-08-29)

## Goal

Accept the 6-hex-digit `ATSH` argument form used for 29-bit CAN headers, so
scanner apps can retarget diagnostic requests to a specific ECU on a 29-bit
bus.

## Background

Field log `elm-logs.txt` (2026-08-29, Car Scanner 2.1.50 against a Honda
Hybrid profile) shows the ESP-OBD adapter rejecting every `ATSH` call the app
sends once it moves past the generic `0100`-style requests:

```
>ATSHDA11F1
?

>ATSHDBEFF1
?

>ATSHDB33F1
?
```

A second session in the same log, against a genuine ELM327-compatible
adapter (reporting `ELM327 v1.5`) on the same phone/profile, shows the
identical commands accepted:

```
>ATSHDA11F1
OK

>ATSHDB33F1
OK
```

`Car Scanner` uses this 6-digit shorthand (`DA11F1`, `DBEFF1`, `DB33F1`, ...)
to mean "29-bit ID `18` + these three bytes" -- i.e. `18DA11F1`, `18DBEFF1`,
`18DB33F1` -- when retargeting UDS/manufacturer-specific requests (mode `22`)
at a specific ECU instead of the default functional broadcast header. Because
ESP-OBD's parser only recognizes 3-hex (11-bit) or 8-hex (29-bit) arguments
(`parseCanIdArgument` in
[at_commands_addressing.cpp](../../src/elm/at_commands_addressing.cpp)),
every 6-digit `ATSH` is rejected with `?` and, per the documented contract for
malformed commands, leaves `session.customHeaderId` unchanged. Every
subsequent physically-addressed request in that part of the log is therefore
still going to whatever header was previously set, not the ECU the app
intended -- a plausible cause of the app reporting ESP-OBD as a "bad
adapter," since retargeting is one of the first capabilities a scanner probes
after the initial `0100` handshake.

This is a genuine gap, not one of the log's other differences, which are
intentional per [ELM_COMMAND_BEHAVIOR.md](../ELM_COMMAND_BEHAVIOR.md):
`ATRV` (no voltage-sense hardware) and `ATAL` (not in the supported command
set) both correctly return `?` on ESP-OBD, matching section 3's explicitly
unsupported list.

## Scope

- Extend the `SH` branch of `dispatchAddressingCommand` (and its shared
  `parseCanIdArgument` helper) to accept a 6-hex-digit argument.
- A 6-digit argument is only meaningful once a 29-bit protocol is selected
  (`ATSP7`/`ATSP9`, or auto-search having connected to one); reject it
  (`?`, no state change) under an 11-bit protocol, consistent with the
  existing "malformed commands do not mutate state" rule.
- When accepted, store the header as `0x18000000 | (value & 0xFFFFFF)`,
  matching the observed genuine-adapter behavior and the `18DAxxxx` /
  `18DBxxxx` request IDs already used elsewhere in this codebase.
- Update [ELM_COMMAND_BEHAVIOR.md](../ELM_COMMAND_BEHAVIOR.md) section 2.3's
  `ATSH` row to document all three accepted argument widths (3, 6, 8 hex
  digits) and the priority-byte behavior of the 6-digit form.

## Steps

1. Add a failing native unit test in
   [test_elm_parser/test_main.cpp](../../test/unit/test_elm_parser/test_main.cpp)
   and/or the addressing contract suite: `ATSH` with a 6-digit argument is
   accepted (`OK`) under a 29-bit protocol and produces the expected
   `0x18......` header on the next transmitted frame.
2. Add the companion negative test: the same 6-digit argument under an
   11-bit protocol returns `?` and leaves `customHeaderId` unchanged.
3. Implement the parsing/storage change in `at_commands_addressing.cpp`.
4. Update the behavior contract doc's `ATSH` row and section 5's status
   notes.
5. Re-run the full native suite and the target build.

## Acceptance criteria

- `ATSHhhhhhh` (6 hex digits) returns `OK` and sets a 29-bit header of
  `0x18hhhhhh` while a 29-bit protocol is active.
- The same command under an 11-bit protocol returns `?` with no state
  change, matching the general malformed-command contract.
- Existing 3-digit and 8-digit `ATSH` behavior is unchanged.
- `docs/ELM_COMMAND_BEHAVIOR.md` accurately documents all three forms.
- `pio test -e native_test` passes with the new cases; `pio run -e
  ioxesp32` still builds.

## Tests

- Unit: 6-digit `ATSH` accepted under `ATSP7`/`ATSP9`, header value exact.
- Unit: 6-digit `ATSH` rejected (`?`, unchanged state) under `ATSP6`/`ATSP8`.
- Regression: existing 3-digit and 8-digit `ATSH` cases still pass unchanged.

## Notes

- Source evidence: `C:\Users\chinn\Downloads\elm-logs.txt`, sessions started
  `29.08.2026 08:26:35` (ESP-OBD) and `29.08.2026 08:28:48` (genuine
  adapter, device name `OBDII`), same phone and Honda/Acura Hybrids profile,
  captured back-to-back for direct comparison.
- Implemented as a new `parseShorthand29BitHeader` helper alongside the
  existing `parseCanIdArgument` in
  [at_commands_addressing.cpp](../../src/elm/at_commands_addressing.cpp);
  the `SH` branch tries the 3/8-digit parser first, then falls back to the
  6-digit shorthand so existing behavior is untouched.
- Not yet confirmed against real hardware/vehicle -- this task was opened
  from log analysis alone and closed on native-test/build evidence only.
  Validate with Car Scanner and a real 29-bit vehicle bus before relying on
  it in the field, per [T10](10-validation-and-release-gate.md)'s standing
  requirement for scanner-app evidence.

Verified: `pio test -e native_test` (160/160 passing, up from 158) and
`pio run -e ioxesp32` still builds (85.8% flash, up slightly from T11's
prior figure).
