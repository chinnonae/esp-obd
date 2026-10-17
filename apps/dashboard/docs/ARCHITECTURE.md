# Dashboard architecture

The dashboard is a dependency-free static single-page app (SPA): plain HTML,
CSS, and native ES modules, no bundler/build step, no backend. It talks to
the ESP-OBD adapter over the Bluetooth SPP virtual COM port using the
browser's [Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API).
See [PLAN.md](PLAN.md) for the originating request and
[DESIGN_DECISIONS.md](DESIGN_DECISIONS.md) for why things are built this way.

## Layout

```
dashboard/
  index.html                – app shell: tab bar, Connect button, view mount point
  manifest.webmanifest      – PWA manifest
  sw.js                     – service worker (offline precache + installability)
  icons/                    – PWA icons
  css/app.css                – app-wide styles
  js/
    app.js                   – bootstrap: wires tabs, Connect button, owns shared app state
    serial.js                 – Web Serial transport + line framing + command queue
    simulator.js               – dev-only fake ELM327/vehicle responder, same interface as serial.js
    elm.js                     – ELM327 init sequence + header-switch helper
    decoder.js                 – OBDb signal decoding + synthetics
    config-store.js            – localStorage-backed signal-profile storage
    poll-engine.js              – polling loop over the active profile's commands
    views/
      current.js                 – live tile grid (current values)
      timeseries.js               – rolling buffers + canvas line chart
      console.js                  – raw TX/RX log
      config.js                   – profile upload/select/delete UI
  data/
    saej1979.json                – bundled generic OBD-II (Mode 01) signal definitions
```

## Data flow

```
Web Serial port
      │  raw bytes
      ▼
  serial.js  ──────────────► console.js (raw TX/RX log, always active)
      │  sendCommand(cmd) -> response text
      ▼
  elm.js (init sequence, ATSH/ATCRA header switching)
      │
      ▼
  poll-engine.js  ── iterates active profile's `commands` in a round-robin loop
      │  raw response bytes for one command
      ▼
  decoder.js  ── extracts each command's `signals` per their `fmt`, then
      │           recomputes any `synthetics` whose inputs just changed
      ▼
  signal update events
      │
      ├──► current.js      (overwrite tile value)
      └──► timeseries.js   (append to that signal's rolling buffer)

config-store.js (localStorage) ──► poll-engine.js (which commands to poll)
                                └─► config.js (profile list/active selection UI)
```

`serial.js` owns the single physical connection and enforces one
command-in-flight at a time (the link is half-duplex request/response — the
adapter processes one line before the next). Every other module reaches the
port only through `serial.js`'s `sendCommand()`.

`simulator.js` is a dev-only stand-in with the identical exported interface
(`connect`/`tryReconnect`/`sendCommand`/`onEvent`/`disconnect`/
`isConnected`), so `elm.js` (which takes its transport as a parameter) and
anything built on top of it work unchanged against either. `app.js` picks
between the two via a "Simulator" toggle in the header — useful for
developing/testing the rest of the app without the real ESP-OBD adapter.

## Module responsibilities

**serial.js** — opens/closes the `SerialPort`, accumulates incoming bytes
until a `>` prompt appears (marking end of response, per
`docs/ELM_COMMAND_BEHAVIOR.md`), strips the trailing CR/prompt, and resolves
the pending `sendCommand()` promise. Also tries `navigator.serial.getPorts()`
on load to reconnect to a previously authorized port without a new user
gesture. Emits `{direction: 'tx'|'rx', text, timestamp}` events for the
console view regardless of whether anything is decoding responses.

**elm.js** — runs the fixed init sequence (`ATZ`, `ATE0`, `ATL0`, `ATH0`) after
connect. Exposes `ensureHeader(hdr, rax)`, which only issues `ATSH`/`ATCRA` if
they differ from what was last set, so a poll loop over many commands sharing
a header doesn't pay for a header switch every request.

**decoder.js** — pure functions: given a command's `fmt`-described `signals`
and the raw response data bytes, extract each signal's scaled value (bit
offset/length, optional byte-swap/sign, `mul`/`div`/`add`, or `map` lookup).
Separately, given the full set of currently-known signal values and a
profile's `synthetics` array, compute any `ratio` derived values whose inputs
are present. No knowledge of transport or storage.

**config-store.js** — wraps `localStorage`. Seeds the bundled
`data/saej1979.json` as a profile on first load, stores uploaded profiles
(keyed by name), tracks which profile is active, and persists across
reloads. This is the only module that talks to `localStorage`.

**poll-engine.js** — given the active profile's `commands`, loops
continuously: for each command, call `elm.js`'s `ensureHeader`, then
`serial.js.sendCommand()` with the concatenated mode+PID, hand the raw
response to `decoder.js`, and publish the decoded values. Runs independent of
which tab is currently visible so timeseries buffers keep filling in the
background.

**views/*.js** — DOM rendering only, subscribing to the events the modules
above emit. No view talks to the serial port or localStorage directly.

## Known limitations

- `fcm1`-flagged commands (multi-frame UDS responses needing ISO-TP flow
  control) are handled best-effort by issuing the adapter's flow-control AT
  commands; this hasn't been validated against real multi-frame traffic on
  hardware.
- Poll rate is bounded by the serial round-trip time and the number of
  commands in the active profile — profiles with many signals (e.g. large EV
  packs) will update slower than a small generic Mode 01 set.
- No support (yet) for the `filter`/model-year fields some OBDb signalsets
  use to scope a command to specific vehicle years — all commands in an
  uploaded profile are polled regardless.
