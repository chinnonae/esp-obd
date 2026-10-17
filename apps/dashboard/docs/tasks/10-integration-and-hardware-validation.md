# D10 - Integration and Hardware Validation

**Status:** Blocked — app fully wired and verified against real hardware for
transport/UI/persistence; signal-level validation needs a real vehicle or
CAN simulator not available in this environment (see Notes)
**Depends on:** [D06](06-current-tab.md), [D07](07-console-tab.md), [D08](08-config-tab.md), [D09](09-timeseries-tab.md)

## Goal

Wire every module together in `app.js`, then validate the whole dashboard
end-to-end against real ESP-OBD hardware and a real vehicle (or CAN
simulator) — the step none of the earlier tasks can fully complete alone.

## Scope

- `dashboard/js/app.js` — final wiring: Connect button drives
  `serial.connect()` → `elm.initSession()` → `poll-engine.start()`; all four
  views mounted and subscribed; Disconnect stops the poll engine and closes
  the port cleanly.
- Update `sw.js`'s precache list to include every file that now exists
  under `dashboard/`.
- Re-verify PWA installability (from [D01](01-app-shell-and-pwa-scaffold.md))
  still holds now that real content exists.

## Steps

1. Wire `app.js` end-to-end per the architecture's data-flow diagram.
2. Update `sw.js`'s precache manifest.
3. Run the full verification checklist below against real hardware.
4. Record actual results (including any deviations from earlier tasks'
   assumptions) in this file's Notes section — this is where surprises from
   real hardware get written down, per the firmware repo's own convention
   (see e.g. `docs/tasks/12-atsh-6digit-header.md`'s Notes section).

## Acceptance criteria / verification checklist

All of these require the real ESP-OBD adapter, paired over Bluetooth
(PIN `5678`), and either a running vehicle or a CAN simulator:

1. Serve `dashboard/`, open in Chrome, confirm install prompt/icon appears.
2. Connect: picker → select the paired COM port → init sequence succeeds →
   Current tab starts populating tiles from the bundled SAEJ1979 profile.
3. Console tab shows raw TX/RX lines matching what's happening.
4. Upload a real vehicle's `default.json` (e.g. from `OBDb/Nissan-Leaf`) on
   Config, switch to it, confirm Current tab now shows that vehicle's
   signals instead.
5. Timeseries tab charts a signal collected over at least a few minutes.
6. Reload the page (or reopen the installed app): uploaded profile and
   active-profile choice persisted.
7. Disconnect cleanly stops polling with no console errors; Connect again
   works without re-pairing.

## Notes

- This task cannot be completed in a sandbox without hardware — it is the
  designated place for that gap, so no earlier task should claim
  hardware-verified status it doesn't have. Update this file (not the
  earlier tasks) once real hardware results are in.
- `app.js` wiring and `sw.js`'s precache list were already kept current
  incrementally across D06–D09 (each task's own Notes record when it was
  updated) — audited 2026-09-05 against the actual `dashboard/` file tree
  and both are complete and accurate as of this task.
- **Verified against real ESP-OBD hardware 2026-09-05** (checklist items 1,
  2 (transport-only), 3, 6, 7):
  1. Manifest parses correctly (both icon sizes) and the service worker is
     registered/activated with all 18 current files precached.
  2. Connect → real port → init sequence (`ATZ`/`ATE0`/`ATL0`/`ATH0`/
     `ATSH7E0`/`ATCRA7E8`) succeeds against the bundled SAEJ1979 profile;
     the poll engine sends every command in turn.
  3. Console tab shows every real TX/RX line in order.
  6. Reload preserves the active profile and any uploaded profiles
     (`localStorage`, confirmed across many reloads this session).
  7. Disconnect stops cleanly (no console errors); reconnecting later
     succeeded via `tryReconnect()` without re-pairing Bluetooth.
- **Blocked — items 2 (tile population), 4, 5 need a real vehicle or CAN
  simulator, which this bench setup doesn't have.** The bench ESP-OBD unit
  has no vehicle/CAN simulator attached, so every real PID request returns
  `UNABLE TO CONNECT` — correct adapter behavior, but it means no signal
  ever decodes, so the Current tab never populates tiles and the
  Timeseries tab never gets real data from actual hardware. These code
  paths (decoding, tile rendering, chart rendering, profile-switch
  propagation) are otherwise fully verified against the in-browser
  simulator in D04/D05/D06/D08/D09 — including uploading and activating a
  real `OBDb/Nissan-Leaf` signalset in D08 — so the only missing piece is
  a genuine vehicle/CAN simulator answering real requests. Whoever has
  access to one should: connect for real, confirm Current tab tiles
  populate from actual vehicle data, upload+activate a real vehicle
  profile and confirm its (real, not simulated) signals appear, and let
  Timeseries collect for several minutes.
- Index left as `[!]` (blocked on real vehicle/CAN-simulator hardware, not
  a code defect) rather than `[x]`, per this file's own instruction not to
  claim hardware-verified status the sandbox can't back up.
