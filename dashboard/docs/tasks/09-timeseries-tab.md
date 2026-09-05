# D09 - Timeseries Tab

**Status:** Planned
**Depends on:** [D05](05-poll-engine.md)

## Goal

Per-signal rolling history and a hand-rolled canvas line chart, so trends
(not just instantaneous values) are visible — per
[DESIGN_DECISIONS.md](../DESIGN_DECISIONS.md#hand-rolled-canvas-line-chart-not-a-vendored-charting-library),
no external charting library.

## Scope

- `dashboard/js/views/timeseries.js`:
  - Maintains a rolling buffer per signal id (e.g. last N samples or last X
    minutes, whichever is simpler to bound memory), fed from
    `poll-engine.onUpdate` in the background regardless of which tab is
    active.
  - A signal picker (dropdown or list) to choose which signal(s) to chart.
  - A `<canvas>`-based line chart: axes, gridlines, a plotted line, redrawn
    on a animation-frame or timer tick — not full DOM re-render per sample.
  - A time-window control (e.g. last 1 / 5 / 15 minutes).

## Steps

1. Implement the rolling buffer (shared module-level state so it keeps
   collecting even while another tab is visible — per the architecture,
   timeseries buffers run in the background).
2. Implement the canvas chart: given an array of `{timestamp, value}` and a
   time window, draw axes + line.
3. Wire the signal picker and time-window control to redraw.

## Acceptance criteria

- Switching to this tab after being on Current for a while shows history
  that was already being collected (not starting from zero at tab-switch
  time).
- Chart redraws smoothly without leaking a growing number of canvas
  contexts or event listeners on repeated tab switches.
- Changing the time window changes the visible range without discarding the
  underlying buffer.

## Verification

Hardware-in-the-loop, manual: connect, let it poll for a few minutes on
Current, switch to Timeseries, confirm a chart with several minutes of
history appears immediately.

## Notes

- Keep the rolling-buffer store separate from the rendering code within
  this module so a future task could swap the chart implementation without
  touching data collection.
