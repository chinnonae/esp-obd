# Dashboard implementation plan

This records the plan as approved, before implementation. It is a snapshot of
intent, not living documentation — see [ARCHITECTURE.md](ARCHITECTURE.md) for
the current design and [DESIGN_DECISIONS.md](DESIGN_DECISIONS.md) for the
rationale behind choices made along the way.

## Context

ESP-OBD is a Bluetooth Classic ELM327-emulating CAN bridge ([../../README.md](../../README.md))
with no WiFi/HTTP support — it just relays ELM327 AT commands and raw OBD hex
requests between a paired host and the vehicle's CAN bus. There was no
existing dashboard. The user asked for a browser-only tool (no backend/server)
that:

1. Shows live OBD-II Mode 01 "current data" (RPM, speed, coolant temp, etc.)
2. Can load a vehicle-specific signal definition file from the
   [OBDb](https://github.com/OBDb) GitHub organization (schema at
   [OBDb/.schemas](https://github.com/OBDb/.schemas)) to decode
   manufacturer-specific PIDs (e.g. EV battery data) in addition to generic
   Mode 01
3. Persists the uploaded config across sessions via browser storage (no
   server)
4. Has a console tab showing raw request/response traffic with the adapter
5. Will eventually support derived/synthetic values computed from other
   signals (can land later)
6. Has two dashboard views: current-value tiles, and a time-series/chart view
7. Is a PWA — installable from the browser as a standalone app

Connection is over the Bluetooth SPP virtual COM port the user pairs manually
(PIN `5678`); no firmware changes.

## Key research findings

**ELM327 protocol** (from `src/`, `docs/ELM_COMMAND_BEHAVIOR.md`): commands
terminate with bare `\r`; echo is on by default (send `ATE0` first); responses
end with `\r\r` then a `>` prompt — buffer until `>`, strip trailing CRs.
Default formatting (headers off, spaces on) gives clean `41 0C 1A F8`-style
lines. Any even-length non-AT hex string (`0100`, `010C`, `221234`, ...) is
passed straight through to the CAN bus. `ATSH<hdr>` sets the request header
(3/6/8 hex-digit forms all supported per T12), and `ATCRA<addr>` filters to
one expected response address.

**OBDb signalset schema** (fetched from `OBDb/.schemas/signals.json` and
cross-checked against real files in `OBDb/SAEJ1979` and `OBDb/Nissan-Leaf`,
both CC BY-SA 4.0):

```jsonc
{
  "commands": [
    { "hdr": "7E0", "rax": "7E8", "cmd": {"01": "0C"}, "freq": 0.25,
      "signals": [
        {"id": "RPM", "path": "Engine.Generic", "name": "Engine RPM",
         "fmt": {"len": 16, "max": 16383.75, "div": 4, "unit": "rpm"}}
      ]}
    // mode 22 (manufacturer UDS) example, 2-byte PID, needs flow control:
    // { "hdr": "743", "rax": "763", "fcm1": true, "cmd": {"22": "0E01"}, "freq": 1, "signals": [...] }
  ],
  "signalGroups": [ /* optional, groups of related signal ids */ ],
  "synthetics": [ /* optional, {id,name,path,max,unit,formula:{op:"ratio",a,b}} computed from other signals */ ]
}
```

- Request bytes = concatenation of the `cmd` object's key+value (mode + PID),
  e.g. `{"01":"0C"}` → `010C`, `{"22":"0E01"}` → `220E01`.
- `fmt.bix` = starting bit offset into the response data bytes (after the
  `41 <pid>`/`62 <pid>` echo), `len` = bit length, optional `blsb`
  (byte-swapped), `sign`, `mul`/`div`/`add` for scaling, or `map` for enums.
  Missing `bix` means offset 0.
- `fcm1: true` signals typically need ISO-TP flow control tuned
  (`ATFCSM`/`ATCFC`) — best-effort passthrough for now, not fully verified
  against hardware.
- Extra fields we don't understand (`dbgfilter`, model-year filters, etc.) are
  simply ignored by the decoder — the schema is meant to be read tolerantly.

Since the exact same schema already covers generic OBD-II, **the built-in
default profile can just be OBDb's own `SAEJ1979` signalset**, bundled
verbatim (with CC BY-SA attribution) as `dashboard/data/saej1979.json`. This
means one decode/poll engine serves both "Mode 01" and any uploaded
vehicle-specific file — no separate hand-written PID table needed.

## Design (as planned)

New top-level folder `dashboard/`, a dependency-free static SPA (native ES
modules, no bundler/npm required) so it stays installable/offline without a
build step.

```
dashboard/
  index.html                  – app shell: tab bar (Current | Timeseries | Console | Config), Connect button
  manifest.webmanifest         – PWA manifest (name, icons, standalone display, start_url/scope)
  sw.js                        – service worker: precache app shell + bundled data for offline/installable use
  icons/icon-192.png, icon-512.png
  css/app.css
  js/
    app.js                     – bootstraps tabs, wires Connect button, owns app state
    serial.js                  – Web Serial open/close, byte-accumulate-until-'>' framing, single-in-flight sendCommand() queue, auto-reconnect via navigator.serial.getPorts()
    elm.js                     – init sequence (ATZ/ATE0/ATL0/ATH0), header-switch helper (only issues ATSH/ATCRA when hdr/rax differs from the last command sent, to minimize round trips)
    decoder.js                 – OBDb fmt decoding (bit extraction, sign/blsb, mul/div/add, map) + synthetics "ratio" support
    config-store.js            – localStorage-backed profile storage: bundled SAEJ1979 profile + any uploaded vehicle profiles, active-profile selection, all persisted
    poll-engine.js             – round-robin polling loop over the active profile's commands, grouping by (hdr,rax) to avoid redundant ATSH calls; emits decoded signal updates + raw TX/RX events
    views/
      current.js               – live tile grid, one tile per signal (name, value, unit), grouped by `path`
      timeseries.js             – rolling per-signal sample buffers + a small hand-rolled canvas line-chart (no external chart lib, keeps the PWA fully offline-installable), selectable signal + time window
      console.js                – scrollable TX/RX log with timestamps (ring-buffered, e.g. last 1000 lines), clear button
      config.js                 – file-picker upload for an OBDb `default.json`, list/select/delete stored profiles, shows which is active
  data/
    saej1979.json               – bundled copy of OBDb/SAEJ1979's signalset (generic Mode 01), CC BY-SA 4.0, attributed in-file and in a footer link on the Config tab
```

**Flow:**

1. On load, `config-store.js` seeds localStorage with the bundled SAEJ1979
   profile if nothing's stored yet, and restores whichever profile was last
   active.
2. Connect tab: Web Serial picker → open port → ELM327 init sequence via
   `elm.js`.
3. `poll-engine.js` starts a loop over the active profile's `commands`,
   sending each request in turn (switching `ATSH`/`ATCRA` only on change),
   decoding responses via `decoder.js`, and fanning updates out to whichever
   view is open plus the timeseries buffers (which run in the background
   regardless of active tab).
4. Console tab subscribes to raw TX/RX events from `serial.js` independent of
   decoding, so it's useful for debugging even against an unknown/malformed
   profile.
5. Config tab lets the user upload a new OBDb `default.json` (minimal
   validation: has a `commands` array), stores it in localStorage, and switch
   the active profile — the poll engine picks up the new command list on next
   loop iteration.
6. Synthetics (`ratio` formula) are computed by `decoder.js` from
   already-decoded signal values whenever both operands are fresh — landed
   now since the schema support is small, but tiles for synthetics can be
   hidden behind a toggle since the user's original ask treated derived
   values as a later addition.

**PWA installability:** `manifest.webmanifest` + `sw.js` (cache-first
precache of the whole `dashboard/` static asset set) make the page
installable — this requires serving over `https://` or `http://localhost`
(Web Serial + service workers refuse plain `file://`). No backend/business
logic involved — just static file hosting (e.g. `npx serve dashboard`,
GitHub Pages, or any static host); all persistence remains 100% client-side
(`localStorage`).

## Verification (as planned)

1. No firmware files are touched — existing `pio run` / `pio test` builds are
   unaffected.
2. Serve `dashboard/` locally (e.g. `npx serve dashboard` or
   `python -m http.server` from that folder) and open it in Chrome/Edge;
   confirm the install prompt/icon appears (manifest + service worker
   registered, no console errors).
3. Pair `ESP-OBD` over Bluetooth (PIN `5678`) so it shows as a COM port;
   click Connect, pick that port, confirm the ELM327 init handshake succeeds
   and the Current tab starts populating tiles from the bundled SAEJ1979
   profile.
4. Upload a real vehicle's `default.json` (e.g. downloaded from
   `OBDb/Nissan-Leaf`) on the Config tab, switch to it, and confirm the
   Current tab now shows that vehicle's signals instead.
5. Check the Console tab shows raw TX/RX lines matching what's happening,
   and the Timeseries tab charts a signal over time.
6. Reload the page (or reopen the installed app) and confirm the uploaded
   profile and active-profile choice persisted via localStorage.
7. Note: full end-to-end validation against a real vehicle/CAN bus can't be
   done in a sandbox without hardware — steps 3–6 need to be run by the user
   with real hardware.
