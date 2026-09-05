# D12 - Custom PID Discovery with Incremental Backoff and NO DATA Toggle

**Status:** Planned

## Goal

Implement intelligent discovery of custom PIDs (from OBDb or user-uploaded
profiles) with incremental backoff for commands that return `NO DATA`, and
allow users to toggle the display of unresponsive PIDs in the dashboard.

## Background

The current polling engine discovers SAEJ1979 Mode 01 PIDs at startup using
standard discovery commands (0100, 0120, etc.), which identify exactly which
PIDs the vehicle supports. However, custom profiles (vehicle-specific OBDb
profiles, UDS Mode 22/23, manufacturer-specific requests) have no built-in
discovery mechanism.

Current behavior:
- Custom commands returning `NO DATA` are skipped for that poll pass
- They are tried again every pass indefinitely (inefficient, wastes bandwidth)
- No feedback to user about which commands are unresponsive
- Adds unnecessary latency to every poll cycle

Required behavior:
- Test each custom command once at startup
- Commands returning `NO DATA` use incremental backoff
  (retry after 10s, 30s, 1m, 5m, then give up)
- User can toggle checkbox to show/hide `NO DATA` PIDs
- Active (working) and inactive (NO DATA) PIDs are visually distinct

## Scope

1. **Discovery engine (`dashboard/js/pid-discovery.js`):**
   - Add `discoverCustomCommands(serial, elm, commands)` function
   - Test each command once; collect results into two sets
   - Return `{ working: Set<command>, noData: Set<command> }`
   - Handle errors gracefully (timeout, malformed, etc.)

2. **Poll engine (`dashboard/js/poll-engine.js`):**
   - Run custom discovery on startup before main polling loop
   - For Mode 01: use existing discovery (0100, 0120, etc.)
   - For other modes: run `discoverCustomCommands()` on those commands
   - Track backoff state for each NO DATA command
   - Skip NO DATA commands unless their backoff window has expired
   - Re-test expired commands; if working, move to active set

3. **Discovery state store:**
   - Extend config-store or create new module
   - Track per-profile: `{ working: Set, noData: Map<cmd, metadata> }`
   - Store in localStorage: `esp-obd-dashboard:discovery-state`
   - Clear on profile switch or manual discovery reset

4. **Dashboard UI (`dashboard/js/views/current.js`, `config.js`):**
   - Add checkbox in header or Config tab: "Show NO DATA Commands"
   - When checked: display NO DATA signals with muted/greyed styling
   - When unchecked: hide them entirely
   - Show badge: "42 signals (8 no-data)" to indicate hidden count
   - Optionally add tooltip on NO DATA tiles: "Last response: NO DATA"

5. **Console tab (`dashboard/js/views/console.js`):**
   - Log discovery results at startup:
     `"Discovery: 42 working, 8 returning NO DATA"`
   - Log backoff events:
     `"Retrying custom-cmd-22F1 (last: 30s ago)..."`

## Acceptance Criteria

- [ ] Custom discovery runs at startup, classifying commands into working/NO DATA
- [ ] Backoff timers (10s, 30s, 1m, 5m, never) are correctly enforced
- [ ] Re-test attempts occur at backoff interval boundaries
- [ ] User can toggle NO DATA visibility via checkbox
- [ ] NO DATA signals are styled distinctly when visible
- [ ] Discovery results persist in localStorage across page reloads
- [ ] Switching profiles resets discovery state
- [ ] Console shows discovery progress and backoff events
- [ ] All existing poll-engine tests pass

## Test Plan

1. **Unit tests (`dashboard/test/`):**
   - `discoverCustomCommands()` with mock serial
   - Backoff timer logic and scheduling
   - Discovery state persistence to/from localStorage
   - Visibility toggle behavior

2. **Manual - Simulator:**
   - Upload custom profile with mix of working and NO DATA commands
   - Observe console log: "Discovery: X working, Y no-data"
   - Toggle "Show NO DATA" checkbox → verify UI updates
   - Reload page → verify discovery results are restored from localStorage

3. **Manual - Real hardware:**
   - Use vehicle-specific OBDb profile (e.g., Nissan Leaf Mode 22 commands)
   - Observe which Mode 22 commands return data vs. NO DATA
   - Wait for backoff timers to expire, observe re-test in console
   - Verify NO DATA commands eventually stabilize

## Dependencies

- D02 (Config store) - for profile storage
- D05 (Poll engine) - main polling loop that this extends
- D07 (Console tab) - for logging discovery events

## Implementation Notes

### Backoff Schedule
- 1st NO DATA → retry after 10s
- 2nd NO DATA → retry after 30s (cumulative: 40s)
- 3rd NO DATA → retry after 60s (cumulative: 100s)
- 4th NO DATA → retry after 300s (cumulative: 400s)
- 5th NO DATA → give up, stop retrying

### Custom vs. Standard Discovery
- Mode 01 (0x01): use existing bitmask discovery (0100, 0120, ..., 01A0)
- Mode 22/23/2D/3E/vendor: test each command individually
- Heuristic: if `mode !== "01"`, use custom discovery

### Signal Metadata Extension
- Add to tile: status indicator (🟢 working, 🔴 NO DATA, 🟡 retrying)
- Tooltip on hover: "Last response: NO DATA (retry in 45s)"
- On click: show full response history for that signal

### localStorage Schema
```javascript
// esp-obd-dashboard:discovery-state
{
  "saej1979": {
    "working": ["01", "04", "05", "0C", ...],
    "noData": {
      "2101": { "failCount": 2, "lastRetry": 1693833600000, "nextRetry": 1693833660000 },
      "2102": { "failCount": 1, "lastRetry": 1693833500000, "nextRetry": 1693833600000 },
      ...
    }
  },
  "uploaded:nissan-leaf": { ... }
}
```

### Visibility Toggle Storage
- Persist toggle state in localStorage: `esp-obd-dashboard:show-no-data`
- Default: unchecked (hide NO DATA signals)

## Future Enhancements (out of scope)

- Auto-disable persistent NO DATA commands (no response ever)
- "Refresh discovery" button to manually re-test all NO DATA commands
- Analytics: track which commands are flaky vs. always dead
- Adaptive backoff: based on command history or response patterns
- Smart backoff: reduce backoff for previously-working commands that suddenly fail
