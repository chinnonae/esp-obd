# ELM327 Init Sequence and Discovery Flow

This document shows the complete request/response sequence for dashboard initialization and PID discovery.

## Complete Session Flow

```
┌─────────────────────────────────────────────────────┐
│          INITIALIZATION PHASE (ELM327)              │
└─────────────────────────────────────────────────────┘

1. Adapter Reset
   → ATZ
   ← ELM327 v1.5a

2. Disable Echo
   → ATE0
   ← OK

3. Disable Line Feeds
   → ATL0
   ← OK

4. Disable Headers
   → ATH0
   ← OK

┌─────────────────────────────────────────────────────┐
│          PROTOCOL SELECTION (Optional)              │
└─────────────────────────────────────────────────────┘

5. Auto-search Protocol
   → ATSP0
   ← OK

┌─────────────────────────────────────────────────────┐
│      MODE 01 PID DISCOVERY (SAEJ1979)               │
└─────────────────────────────────────────────────────┘

6. Setup broadcast header
   → ATSH18DB         (broadcast sender ID)
   ← OK

7. Setup receive address
   → ATCRA33F1        (broadcast receiver address)
   ← OK

8. Query PIDs 01-20 support
   → 0100
   ← 41 00 BE 1F A8 01
      │      └─────────── Bitmask: PIDs 01, 03, 04, 05, ...

9. Query PIDs 21-40 support
   → 0120
   ← 41 20 00 00 00 01
      │      └─────────── Few PIDs supported in this range

10. Query PIDs 41-60 support
    → 0140
    ← 41 40 FF FF FF FF
       │      └─────────── All PIDs 41-60 supported

11. Query PIDs 61-80 support
    → 0160
    ← 41 60 00 00 00 00
       │      └─────────── No PIDs in this range

12. Query PIDs 81-A0 support
    → 0180
    ← 41 80 00 00 00 00

13. Query PIDs A1-C0 support
    → 01A0
    ← 41 A0 00 00 00 00

Result: Supported PIDs set = {01, 03, 04, 05, 06, 07, 08, 09, 0B, 0C, 0D, 0E, 0F, 
                                10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 1A, 20,
                                41, 42, 43, 44, 45, 46, 47, 48, 49, 4A, 4B, 4C, ...}

┌─────────────────────────────────────────────────────┐
│     MODE 22 CUSTOM COMMAND DISCOVERY (Honda)        │
└─────────────────────────────────────────────────────┘

14. Setup Mode 22 header (Honda profile)
    → ATSHDA01
    ← OK

15. Setup receive address
    → ATCRAF1
    ← OK

16. Test battery cell voltages
    → 222028
    ← 62 28 04 D5 04 C8 04 CA 04 CB 04 CA 04 C7 04 C3
       │      └─────────────────────────────────────── Cell voltage data

17. Test SOC/battery metrics
    → 22202A
    ← 62 2A 64 01 C8 D0 01 08 00
       │      └────────────────── SOC + voltage + current data

Result: Working commands = {2028, 202A}
        NO DATA commands = {} (or any custom commands that don't respond)

┌─────────────────────────────────────────────────────┐
│         POLLING PHASE (Continuous)                  │
└─────────────────────────────────────────────────────┘

18. Poll Mode 01 commands (one per cycle)
    → 010C
    ← 41 0C 00 10  (RPM)

19. Poll Mode 01 commands (next in cycle)
    → 010D
    ← 41 0D 58     (Speed)

... continue round-robin through all supported PIDs

20. Periodically poll Mode 22 custom commands
    → 222028
    ← 62 28 04 D5 04 C8 ... (battery cells)

... continue until disconnect
```

## Timing

| Phase                  | Commands | Time (approx) |
|------------------------|----------|---------------|
| Initialization         | 4        | 200-400ms     |
| Protocol selection     | 1        | 50-100ms      |
| Mode 01 header setup   | 2        | 100-200ms     |
| PID discovery (6 cmds) | 6        | 300-600ms     |
| Mode 22 header setup   | 2        | 100-200ms     |
| Custom discovery       | N        | 50-200ms each |
| **Total startup**      | **15+N** | **~1-2 sec**  |

## Troubleshooting

### Discovery hangs on 0140
- Check if device supports Mode 01 commands
- May indicate CAN bus not initialized
- Try `ATSP0` to force auto-search

### All responses show "NO DATA"
- Header/address may be wrong
- CAN bus speed mismatch
- Vehicle not responding on broadcast address

### Mode 22 commands timeout
- Device may not support UDS (Mode 22)
- Custom command discovery will mark as NO DATA after timeout
- Polling will skip with backoff

## Mock Testing

Use the provided mock adapters to test without hardware:

```javascript
// In browser console:
import('./js/serial-mock.js').then(m => m.runMockSession());

// Or in Node.js:
import('./test-elm-mock.js').then(m => m.simulateFullSession());
```

This will simulate the complete sequence with realistic timing and validate the discovery logic.
