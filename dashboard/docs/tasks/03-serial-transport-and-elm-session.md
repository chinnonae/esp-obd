# D03 - Serial Transport and ELM327 Session

**Status:** Blocked — implemented and verified against an in-browser
simulator; real hardware-in-the-loop verification still needed (see Notes)
**Depends on:** [D01](01-app-shell-and-pwa-scaffold.md)

## Goal

A working Web Serial connection to the real ESP-OBD adapter: connect, run
the ELM327 init sequence, and send/receive one command at a time with
correct framing. This is the riskiest module (only verifiable against real
hardware) so it's built and validated on its own before the poll engine
depends on it.

## Background

Protocol facts (from [PLAN.md](../PLAN.md), sourced from `src/` and
`docs/ELM_COMMAND_BEHAVIOR.md` in the firmware repo):

- Commands terminate with a bare `\r`.
- Echo is on by default — send `ATE0` early.
- Responses end with `\r\r` (L0) then a `>` prompt; buffer received bytes
  until `>` appears, then strip trailing CRs/prompt to get the response text.
- `ATSH<hdr>` sets the request header (3/6/8 hex-digit forms), `ATCRA<addr>`
  filters the expected response address.

## Scope

- `dashboard/js/serial.js`:
  - `connect()` — `navigator.serial.requestPort()` + `port.open(...)`,
    starts the read loop.
  - `tryReconnect()` — called on app load, uses `navigator.serial.getPorts()`
    to reopen a previously authorized port without a new user gesture.
  - `sendCommand(text)` — writes `text + '\r'`, returns a Promise resolved
    with the parsed response text once a `>` is seen; enforces one
    in-flight command at a time (queue further calls).
  - `onEvent(callback)` — subscribe to `{direction: 'tx'|'rx', text,
    timestamp}` events for every line sent/received, independent of whether
    anything consumes the parsed response (this is what
    [D07](07-console-tab.md) will use).
  - `disconnect()` — closes the port, stops the read loop.
- `dashboard/js/elm.js`:
  - `initSession(serial)` — runs `ATZ`, `ATE0`, `ATL0`, `ATH0` in order,
    throws/rejects with a clear error if any step doesn't return `OK`
    (`ATZ`'s response is the adapter banner, not `OK` — treat any
    non-error response as success for that step specifically).
  - `ensureHeader(serial, hdr, rax)` — tracks the last `(hdr, rax)` sent;
    only issues `ATSH<hdr>` / `ATCRA<rax>` if either differs from last time.

## Steps

1. Implement `serial.js`'s byte-accumulation/framing and command queue.
2. Implement `elm.js`'s init sequence and header-switch helper on top of it.
3. Wire `app.js`'s Connect button (stubbed in D01) to `serial.connect()` +
   `elm.initSession()`, showing connection status in the header.
4. Manually test against the real ESP-OBD adapter (see Verification) — this
   is the first task in the board that touches real hardware.

## Acceptance criteria

- Clicking Connect prompts the Chrome serial port picker; selecting the
  paired `ESP-OBD` port succeeds and the init sequence completes.
- `sendCommand('0100')` returns the vehicle's actual supported-PID response
  (or a `NO DATA`/`?` if no vehicle/simulator is attached — verified as a
  clean rejection, not a hang or a parse error).
- Reloading the page and clicking Connect again works without needing to
  unpair/re-pair Bluetooth.
- Sending two commands back-to-back without awaiting the first doesn't
  interleave their responses (queue behaves correctly) — test by firing two
  `sendCommand()` calls without `await` and confirming both resolve with the
  right, non-mixed text.

## Verification

Hardware-in-the-loop, manual:

1. Pair `ESP-OBD` over Bluetooth (PIN `5678`) so it appears as a COM port.
2. Serve `dashboard/` locally, open in Chrome, click Connect, pick that port.
3. Confirm the init sequence completes (log to console for now — D07 gives
   this a real UI).
4. With a vehicle or CAN simulator attached, `sendCommand('010C')` should
   return an `41 0C xx xx`-shaped response.

This cannot be verified without real hardware; note results (or blockers) in
this file's Notes section once run.

## Notes

- If the init sequence's `ATZ` response format differs from what's assumed
  here once tested against real hardware, fix this task file's assumption
  rather than silently special-casing it in code only.
- Added `dashboard/js/simulator.js`, an in-browser fake ELM327 + vehicle
  responder that implements the exact same interface as `serial.js`
  (`connect`/`tryReconnect`/`sendCommand`/`onEvent`/`disconnect`/
  `isConnected`). `app.js` picks either it or the real `serial.js` based on
  a "Simulator" checkbox in the header, and passes whichever into
  `elm.initSession()`/`ensureHeader()` — those two functions already took
  their transport as a parameter, so no change was needed there. This isn't
  a substitute for the real hardware-in-the-loop verification below, but it
  let the command queue, init sequence, and error paths (`NO DATA`,
  unsupported PIDs) be exercised without a physical adapter.
- Verified 2026-09-05 (no hardware available in this sandbox):
  - `serial.js`'s real `connect()` correctly rejects with `NotFoundError`
    when no port is picked (clean rejection, not a hang), confirming the
    error path works even though the full picker → real-adapter flow
    couldn't be exercised.
  - Against the simulator: connect, ELM init sequence, `sendCommand` for
    several PIDs (`010C`, `010D`, `0105`, `0100`) returning correctly
    shaped `41 <pid> <data>` responses, an unsupported command returning
    `NO DATA`, two concurrent `sendCommand()` calls resolving with
    non-interleaved responses (queue behaves correctly), and disconnect —
    all via the integrated browser against the served page.
  - **Blocked**: full acceptance against the real ESP-OBD adapter (port
    picker → real init handshake → real `010C` response from a vehicle/CAN
    simulator) needs physical hardware/Bluetooth pairing not available in
    this environment. Whoever runs this next should pair `ESP-OBD` (PIN
    `5678`), serve `dashboard/`, uncheck Simulator, click Connect, and
    confirm the four items in Acceptance criteria above against the real
    device.
