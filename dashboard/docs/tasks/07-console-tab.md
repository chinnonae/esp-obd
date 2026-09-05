# D07 - Console Tab

**Status:** Done (2026-09-05)
**Depends on:** [D03](03-serial-transport-and-elm-session.md)

## Goal

A scrollable, timestamped log of every raw request/response line exchanged
with the adapter — useful for debugging a profile or the adapter itself,
independent of whether decoding succeeds.

## Scope

- `dashboard/js/views/console.js`:
  - Subscribes to `serial.onEvent` (`{direction, text, timestamp}`) directly
    — not through the poll engine or decoder, so it stays useful even if a
    profile is malformed or nothing decodes.
  - Ring-buffers the last ~1000 entries (drop oldest on overflow) to bound
    memory on long sessions.
  - Renders TX and RX lines distinctly (e.g. different color/prefix), with
    a relative or wall-clock timestamp.
  - A "Clear" button empties the buffer and the rendered list.

## Steps

1. Subscribe to `serial.onEvent` and append to an in-memory ring buffer.
2. Render efficiently — append new lines rather than re-rendering the whole
   list each event; auto-scroll to bottom unless the user has scrolled up.
3. Wire the Clear button.

## Acceptance criteria

- Every command sent and every response received appears here, matching
  what D03/D05 actually transmit (spot-check against a couple of manual
  `sendCommand` calls).
- Long sessions (>1000 lines) don't grow memory unbounded — oldest entries
  are dropped.
- Clear empties the view immediately.

## Verification

Hardware-in-the-loop, manual: connect, watch the console tab populate as the
poll engine runs, confirm TX/RX pairing looks correct (each TX line
immediately followed by its RX response).

## Notes

- This view has no dependency on the decoder or config store — keep it that
  way, it's meant to work even when profile-based decoding doesn't.
- Subscribes to both `serial.onEvent` and `simulator.onEvent` (only
  whichever is actually connected ever emits) so it works regardless of
  which transport `app.js` chose, without needing to know that choice.
- Verified 2026-09-05 against the real adapter (auto-reconnected cleanly
  via `tryReconnect`): every TX/RX line from the real init sequence and
  poll-engine's requests appeared in order, each TX immediately followed by
  its RX (e.g. `TX 0101` at `.354` → `RX UNABLE TO CONNECT` at `28.166`,
  matching the real ~800ms round trip). Clear emptied the view immediately
  and new lines kept appending afterward. Disconnect stopped cleanly with
  no console errors.
- **Regression found later (fixed in D09):** the `.view { display: flex }`
  rule added here for the console log's layout unintentionally overrode
  the browser's default `[hidden] { display: none }`, breaking tab
  switching (inactive views stayed visible). Fixed with an explicit
  `.view[hidden] { display: none; }` rule in `css/app.css`.
