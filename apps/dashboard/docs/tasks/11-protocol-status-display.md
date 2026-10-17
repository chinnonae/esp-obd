# D11 - Protocol Status Display

**Status:** Done

## Goal

Display the current OBD-II protocol (connection mode) on the dashboard so the
user can see which CAN protocol is active and whether the adapter is searching,
connected, or failed.

## Background

The ESP-OBD firmware detects and locks onto a protocol via auto-search (trying
11-bit 500k → 29-bit 500k → 11-bit 250k → 29-bit 250k), but the dashboard
has no visibility into this state. The protocol is determined server-side by
the diagnostic transport and relayed back to the ELM session, but the web UI
does not expose it.

Without this information:
- Users cannot confirm they're on the intended protocol
- Debugging connection issues is harder
- No clear feedback during protocol auto-search phase

## Scope

1. **Dashboard serial.js:**
   - Extend command parsing to capture protocol state from ELM responses
   - Expose via a new `protocolState` property: `{ name, status, timestamp }`
   - Emit `protocolChange` events when state changes

2. **Dashboard elm.js:**
   - Track protocol selection commands (ATSP6-9, SP0 for auto-search)
   - Map back to friendly names (e.g., "11-bit 500k", "29-bit 500k")
   - Maintain protocol state across init sequence

3. **Dashboard UI:**
   - Add protocol status display element in header or separate status bar
   - Show:
     - During search: "🔍 Searching..."
     - When connected: "✓ ISO-TP 11-bit 500k" (or the actual protocol)
     - On failure: "✗ No protocol found" or "⚠ Check connection"
   - Update in real-time as protocol state changes
   - Clear on disconnect/reconnect

4. **Console tab integration:**
   - Log protocol selection when user runs ATSP/ATTP commands
   - Log protocol lock when auto-search succeeds

## Acceptance Criteria

- [ ] Dashboard displays protocol name and connection state in a visible location
- [ ] State updates in real-time as adapter searches and locks onto protocol
- [ ] Display correctly shows all protocol variants (11/29 bit, 500k/250k)
- [ ] State is cleared/reset on disconnect and reconnect
- [ ] Protocol changes are logged to console
- [ ] All existing tests pass (no regression)

## Test Plan

1. **Manual - Simulator:**
   - Connect via simulator → verify initial state
   - Run `ATSP6`, `ATSP7`, `ATSP8`, `ATSP9` in console → verify display updates
   - Disconnect → verify state clears

2. **Manual - Auto-search:**
   - Connect to real hardware with auto-search (SP0)
   - Observe "🔍 Searching..." during attempts
   - Watch protocol lock to discovered mode
   - Verify name matches actual protocol (11/29 bit, 500k/250k)

## Dependencies

- D03 (Serial transport and ELM session) - transport layer exists
- D07 (Console tab) - for logging protocol events

## Implementation Notes

### Protocol Name Mapping
- ATSP6 → "ISO 15765-4 (CAN 11/500)" or "11-bit 500k"
- ATSP7 → "ISO 15765-4 (CAN 29/500)" or "29-bit 500k"
- ATSP8 → "ISO 15765-4 (CAN 11/250)" or "11-bit 250k"
- ATSP9 → "ISO 15765-4 (CAN 29/250)" or "29-bit 250k"
- SP0 → "Auto-search" (during search phase)

### State Extraction
- Protocol is not explicitly reported by ELM327; derive from:
  - Last ATSP/ATTP/TPA/TPB command issued
  - Diagnostic transport auto-search completion (via app status)
  - Implicit in ELM session's `session.protocol` field (if exposed via API)

### UI Placement
- Header area, next to connection status
- Compact format: "✓ 11-bit 500k" or "🔍 Searching..." (20-40px width)
- Tooltip: full protocol name and last update time
