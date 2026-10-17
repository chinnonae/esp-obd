# Design decisions

Rationale for the non-obvious choices behind the dashboard's design. See
[ARCHITECTURE.md](ARCHITECTURE.md) for the resulting structure and
[PLAN.md](PLAN.md) for the original request this was scoped against.

## Bundle OBDb's own SAEJ1979 signalset as the generic Mode 01 profile

Rather than hand-writing a formula table for common PIDs (RPM, speed,
coolant temp, ...), the built-in default profile is a verbatim copy of
[`OBDb/SAEJ1979`](https://github.com/OBDb/SAEJ1979)'s `signalsets/v3/default.json`
(CC BY-SA 4.0, attributed in `dashboard/data/saej1979.json` and on the Config
tab). This means the exact same decode/poll engine handles both generic
Mode 01 and any manufacturer-specific file the user uploads later — there is
only one code path for "interpret an OBDb signalset," not two.

## No server, no build step

The user explicitly ruled out a server for persistence and asked for an
installable SPA. Rather than introduce a bundler (Vite/webpack) "just because
that's normal for an SPA," the app uses native ES modules loaded directly by
the browser. This keeps the whole thing a folder of static files that can be
served by literally anything (`npx serve`, `python -m http.server`, GitHub
Pages) with no build artifact to keep in sync with source.

Note this is a different concern from installability: a PWA's service worker
and Web Serial both refuse to run under a plain `file://` URL, so *some*
static file host is still required to actually use or install the page. That
host serves static files only — no backend logic, no database. All state
(uploaded profiles, active selection) lives in the browser via
`localStorage`.

## localStorage over IndexedDB

The user's own suggestion. Uploaded OBDb signalsets are JSON text, typically
tens of KB and up to roughly 100 KB for the largest EV profiles seen in the
OBDb org — comfortably inside `localStorage`'s per-origin quota (usually
5–10 MB). `localStorage`'s synchronous API is also simpler to reason about
for "save on upload, read on load" than IndexedDB's async transactions,
which would be overkill for infrequent, small writes.

## Hand-rolled canvas line chart, not a vendored charting library

The timeseries view needed for #6 doesn't require anything beyond a line
chart per signal. Vendoring a charting library (even a small one) to satisfy
that would mean either a runtime CDN dependency — which breaks offline
installability, since a cached PWA can't fetch from a CDN when the vehicle
is out of signal range — or committing a vendored copy or npm dependency
purely to draw lines on a canvas. A few dozen lines of `<canvas>` drawing
code was judged simpler and keeps the "no build step" property intact.

## Synthetics implemented now, not deferred

The user's original ask (#5) said derived values could land later, treating
it as the lowest-priority item. Once the OBDb schema was actually read,
though, the only synthetic operation currently defined (`ratio`, i.e.
`a / b`) turned out to be a few lines in `decoder.js` — computed from signal
values the decoder already holds, with no new transport or storage work.
Implementing it now was cheaper than stubbing it out and revisiting the
decoder later, so it's wired in, without a dedicated "add your own formula"
UI (there's exactly one operation to expose, and only for profiles that
define a `synthetics` array).

## Passthrough (not fully implemented) flow control for `fcm1` commands

Some manufacturer-specific (mode `22`) commands in real OBDb signalsets are
flagged `fcm1: true`, meaning their multi-frame UDS responses need ISO-TP
flow control tuned via the adapter's `ATFCSM`/`ATCFC` commands. Getting this
exactly right requires per-vehicle tuning that can't be verified without
real hardware and a real vehicle exhibiting multi-frame responses, so it's
implemented as a best-effort passthrough (issue the flagged AT commands, no
guarantee the framing works for every ECU) rather than blocking the rest of
the dashboard on solving it. Flagged in
[ARCHITECTURE.md](ARCHITECTURE.md#known-limitations).

## Model-year filters ignored

Some OBDb signalsets scope individual commands to specific model years via a
`filter`/`dbgfilter`-style field. The dashboard doesn't ask the user for
their vehicle's year, so it has no basis to apply that filter — every
command in an uploaded profile is polled regardless of year applicability.
Wiring this up would need a "which vehicle year" input in the Config tab,
which wasn't part of the original request.
