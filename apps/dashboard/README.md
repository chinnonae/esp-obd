# ESP-OBD Dashboard

A browser-only, installable dashboard for the [ESP-OBD](../README.md)
adapter. It connects over the Bluetooth SPP virtual COM port using the
[Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)
and shows live OBD-II data — no server, no build step, no firmware changes.

> [!NOTE]
> This is a plan/design snapshot — implementation is in progress. See
> [docs/PLAN.md](docs/PLAN.md) for the full plan,
> [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the module layout, and
> [docs/DESIGN_DECISIONS.md](docs/DESIGN_DECISIONS.md) for why things are
> built this way.

## What it does

- **Current tab** — live tile grid of decoded signal values (RPM, speed,
  coolant temp, etc. by default; whatever an uploaded vehicle profile
  defines otherwise).
- **Timeseries tab** — the same signals charted over time.
- **Console tab** — raw request/response traffic with the adapter, for
  debugging.
- **Config tab** — upload a vehicle-specific signal definition file from the
  [OBDb](https://github.com/OBDb) project (schema:
  [OBDb/.schemas](https://github.com/OBDb/.schemas)) to decode
  manufacturer-specific PIDs (e.g. EV battery data) instead of just generic
  Mode 01. Uploaded profiles persist in the browser (`localStorage`) — there
  is no server component.
- Installable as a standalone app (PWA) from a supporting browser.

## Requirements

- A Chromium-based browser (Chrome or Edge) — Web Serial isn't available in
  Firefox or Safari.
- The ESP-OBD adapter paired over Bluetooth Classic (device name `ESP-OBD`,
  PIN `5678`), so it shows up as a COM port on your machine.
- The dashboard served from `https://` or `http://localhost` — Web Serial
  and the service worker both refuse plain `file://` pages. Any static file
  server works, e.g.:

```powershell
npx serve dashboard
```

or

```powershell
python -m http.server --directory dashboard
```

Then open the printed `http://localhost:...` URL in Chrome/Edge.

## Using a vehicle-specific profile

1. Find your vehicle's repo under [github.com/OBDb](https://github.com/OBDb)
   (e.g. `OBDb/Nissan-Leaf`) and download its
   `signalsets/v3/default.json`.
2. On the Config tab, upload that file.
3. Switch the active profile to it. The Current/Timeseries tabs immediately
   start polling that vehicle's signals instead of the generic set.

The generic profile bundled by default is OBDb's own
[`SAEJ1979`](https://github.com/OBDb/SAEJ1979) signalset (standard OBD-II
Mode 01 "current data"), used under CC BY-SA 4.0.

## Limitations

- Manufacturer-specific commands that need multi-frame ISO-TP flow control
  (`fcm1` in the OBDb schema) are handled best-effort and haven't been
  validated against real hardware.
- Model-year-scoped commands in an uploaded profile are polled regardless of
  year — there's no vehicle-year input yet.
- Poll rate depends on how many signals the active profile defines; large EV
  profiles will update slower than the generic Mode 01 set.
