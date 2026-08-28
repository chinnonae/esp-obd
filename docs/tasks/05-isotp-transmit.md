# T05 - ISO-TP Transmit State Machine

**Status:** Done (started 2026-08-28, completed 2026-08-28)

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

## Notes

- `kMaxPayloadBytes` moved from `isotp_receive.h` into the shared
  `isotp_pci.h` so RX and TX reference one definition. Also added there:
  `stMinToMilliseconds` (0x00-0x7F direct; 0xF1-0xF9 sub-millisecond values
  round up to 1ms, since this project's clock resolution is milliseconds;
  reserved values fail safe to 127ms).
- `IsoTpTransmitter` ([include/isotp/isotp_transmit.h](../../include/isotp/isotp_transmit.h))
  holds a non-owning `const uint8_t*`/length into the caller's buffer per
  Step 1 ("caller-owned immutable request buffer") -- unlike the RX side, TX
  already has the whole payload upfront, so there's nothing to reassemble
  or copy.
- `CAF0` (raw mode) deliberately has no code here: per the task's own
  framing ("never interprets ISO-TP"), the diagnostic layer (T06) should
  call `can::makeStandardFrame`/`makeExtendedFrame` directly with the
  caller's bytes for that case -- T02's factories already do exactly "send
  exactly caller-provided bytes," so nothing new was needed.
- Flow Control block-size/STmin come from the last received FC, not from
  `TxConfig`: ISO-TP has the *receiver* dictate pacing to the sender.
  `TxConfig` only carries our own id/addressing and timeouts.
- New suite `test/unit/test_isotp_transmit/`: exact single-frame encoding
  (including the contract's `010C` worked example), 1- and 7-byte
  boundaries, FF+CF for an 8-byte payload, sequence wrap over 16 CFs, FC
  block-size re-arm, FC Wait/Overflow/missing/malformed, STmin pacing
  advanced entirely through `poll(now)` (no real sleeping), a bus-error
  send failure, and extended addressing plus a 29-bit CAN id.

Verified: `pio test -e native_test` (75/75 passing across seven suites) and
`pio run -e ioxesp32` still builds.
