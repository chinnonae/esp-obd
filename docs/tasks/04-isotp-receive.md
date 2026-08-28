# T04 - ISO-TP Receive State Machine

**Status:** Planned

## Goal

Reliably collect a single responder's ISO-TP response, including automatic or
manual flow control, without blocking the rest of the application indefinitely.

## Scope

- Single Frame, First Frame, Consecutive Frame, and Flow Control parsing.
- Declared-length tracking up to the chosen bounded payload limit.
- Sequence validation, receive timeouts, extended addressing, and padding.
- Header-on raw-frame reporting data in addition to reassembled payloads.

## Steps

1. Define explicit receive states and typed terminal results.
2. Implement `start()` and `poll(now)`; neither function calls `delay()`.
3. On a valid First Frame, choose automatic/manual/no flow control according to
   session settings and queue the correct frame.
4. Reject invalid PCI, wrong sequence, overflow, and mismatched extended
   address without returning a successful partial payload.
5. Expose raw frames and completed payload separately for the ELM formatter.

## Acceptance criteria

- A known VIN-like response is reassembled exactly.
- The timeout behavior is deterministic under `FakeClock`.
- Flow-control ID/data behavior is correct for both 11-bit and 29-bit OBD.
- No receive path reads beyond the configured fixed buffer.

## Tests

- Single-frame payload and padding removal.
- Multi-frame happy path, sequence wrap, missing CF, wrong sequence, and
  declared length larger than the limit.
- Headers-on output data and extended-address matching.
