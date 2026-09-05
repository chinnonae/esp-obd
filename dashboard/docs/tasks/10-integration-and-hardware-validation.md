# D10 - Integration and Hardware Validation

**Status:** Planned
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
