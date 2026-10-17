# D05 - Poll Engine

**Status:** Done (2026-09-05)
**Depends on:** [D02](02-config-store-and-bundled-profile.md), [D03](03-serial-transport-and-elm-session.md), [D04](04-signal-decoder-and-synthetics.md)

## Goal

Continuously poll the active profile's commands over the connected serial
session, decode each response, and publish signal-update events that the
Current and Timeseries tabs (and nothing else) subscribe to.

## Scope

- `dashboard/js/poll-engine.js`:
  - `start(serial, elm, configStore)` — begins a round-robin loop over
    `configStore.getProfile(configStore.getActiveProfileId()).commands`.
  - For each command: call `elm.ensureHeader(serial, command.hdr,
    command.rax)`, then `serial.sendCommand(modeHex + pidHex)`, parse the
    response's data bytes (strip the `41/62 <pid>` echo, handle `NO DATA`/`?`
    as "no update this round" rather than an error), hand the rest to
    `decoder.decodeResponse`.
  - After each full pass, call `decoder.computeSynthetics` against the
    accumulated signal values and merge the result in.
  - `onUpdate(callback)` — subscribe to `{signalId, value, timestamp}`
    events, one per decoded (or synthetic) signal per pass.
  - `stop()` — halts the loop (called on disconnect).
  - Restarts its command list automatically when
    `configStore`'s active profile changes (poll for this via an event or a
    simple "check active id at the top of each pass" — either is fine, pick
    the simpler one).

## Steps

1. Implement the round-robin loop with header-switch minimization (only
   call `ensureHeader` when `(hdr, rax)` differs from the previous
   command in the pass).
2. Implement response-to-data-bytes stripping (remove the echoed mode+PID
   hex, handle multi-line/multi-ECU responses by taking the first line for
   now — note this as a limitation if a real multi-ECU case is hit in
   [D10](10-integration-and-hardware-validation.md)).
3. Wire in synthetics computation after each pass.
4. Add a simple "ms per pass" measurement, exposed for the Current tab's
   status line.

## Acceptance criteria

- Switching the active profile in `configStore` changes what's being polled
  within one pass, without needing to reconnect.
- A `NO DATA` or `?` response for one command doesn't stop the loop from
  continuing to the next command.
- `onUpdate` fires for every signal in the active profile's commands, plus
  any computable synthetics, once per pass.
- Disconnecting the serial port stops the loop cleanly (no unhandled
  rejection spam in the console).

## Verification

Hardware-in-the-loop, manual, once D03 is validated: connect to the real
adapter, start the poll engine, and confirm `onUpdate` events arrive at a
reasonable rate (log to console). Swap the active profile mid-session and
confirm the command set changes.

## Notes

- Depends on D03 being validated against real hardware first — if the
  init-sequence or framing assumptions there change, this task's response
  stripping logic (which assumes clean `41 <pid> <data...>` text) may need
  to change too.
- Verified 2026-09-05 against the simulator (real hardware has no vehicle
  attached on the bench, so every real PID currently returns `UNABLE TO
  CONNECT` -- not useful for exercising decode paths, but real `sendCommand`
  round-trips were already proven in D03):
  - A full pass over the bundled SAEJ1979 profile (103 commands) against
    the simulator correctly decoded every PID it answers (`RPM`, `VSS`,
    `ECT`, `LOAD_PCT`, `TP`, `IAT`, `FLI`) and silently skipped the rest
    (`NO DATA`) without stopping the loop.
  - Switching the active profile mid-session (via `configStore
    .setActiveProfileId`) changed the polled command set within one pass,
    confirmed by the last several `onUpdate` events being exclusively the
    new profile's signal after the switch, with no reconnect.
  - `stop()` + `disconnect()` halted the loop with no unhandled rejections.
- Only the first line of a response is parsed; a genuine multi-ECU reply
  to the same filtered request hasn't been exercised against real hardware
  (the bench unit currently answers with a single-ECU-shaped `ATCRA`
  filter) -- flag this in [D10](10-integration-and-hardware-validation.md)
  if a real multi-line case turns up.
