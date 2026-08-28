# T05 - ISO-TP Transmit State Machine

**Status:** Planned

## Goal

Send all in-scope diagnostic payload sizes correctly, including multi-frame
requests that the legacy implementation could not transmit.

## Scope

- Single Frame generation for payloads up to seven bytes.
- First Frame and Consecutive Frame generation for payloads up to the chosen
  ISO-TP limit.
- Flow Control parsing: continue/wait/overflow, block size, STmin, and timeout.
- Standard and extended CAN IDs, extended addressing, `CAF1`, and raw `CAF0`.

## Steps

1. Define explicit TX states and a caller-owned immutable request buffer.
2. Generate exact PCI/padding bytes for a Single Frame or First Frame.
3. Wait for valid flow control from the expected responder before sending CFs.
4. Honor block size and STmin using `IClock`; never busy-wait.
5. Return a typed result that distinguishes timeout, overflow, and bus failure.

## Acceptance criteria

- `010C` produces one exact standard OBD CAN frame.
- An 8+ byte request produces correct FF/CF sequence and wrap behavior.
- Flow-control timing and block-size behavior are testable without real time.
- `CAF0` sends exactly caller-provided bytes and never interprets ISO-TP.

## Tests

- Single-frame 1-byte and 7-byte payloads.
- Multi-frame payloads spanning sequence wrap and multiple FC blocks.
- FC Wait, Overflow, missing FC, malformed FC, STmin boundaries, and bus error.
