# D06 - Current Tab

**Status:** Planned
**Depends on:** [D05](05-poll-engine.md)

## Goal

Live tile grid showing the current value of every signal in the active
profile, grouped by the OBDb `path` field, plus the poll engine's
ms-per-pass status.

## Scope

- `dashboard/js/views/current.js`:
  - Renders one tile per signal id seen from `poll-engine.onUpdate`
    (name, live value + unit, or the raw mapped string for enum signals).
  - Groups tiles under a heading per top-level `path` segment (e.g.
    `Engine`, `Battery`) so a large vehicle profile doesn't render as one
    flat wall of tiles.
  - Shows the poll engine's status line (ms per full pass).
  - Rebuilds its tile set when the active profile changes (new signal ids
    appear/disappear).

## Steps

1. Subscribe to `poll-engine.onUpdate`; on first sight of a signal id,
   create its tile; on every subsequent event for that id, update the value
   in place (don't recreate DOM nodes per update).
2. Subscribe to whatever event/mechanism D05 uses to signal an active
   profile change, and clear+rebuild the tile set.
3. Style tiles in `css/app.css` (grid layout, grouped headings).

## Acceptance criteria

- Tiles update live without visible flicker or DOM node churn (verify via
  DevTools > Elements that node count is stable while values change).
- Switching profiles in the Config tab immediately reflects in which tiles
  are shown here.
- Enum (`map`-based) signals show their mapped string, not a raw number.

## Verification

Hardware-in-the-loop, manual: connect, confirm tiles populate and update;
switch profile, confirm the tile set changes to match.

## Notes

- No chart/history here — that's [D09](09-timeseries-tab.md). This tab is
  deliberately just "what is it right now."
