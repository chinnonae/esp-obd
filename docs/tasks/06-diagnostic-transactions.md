# T06 - Diagnostic Transactions

**Status:** Planned

## Goal

Combine CAN addressing and ISO-TP state machines into a deterministic service
that executes one ELM diagnostic request and returns structured responders.

## Scope

- Lives in its own `diagnostic/` layer (see [Architecture](../ARCHITECTURE.md)),
  between `isotp/` and `elm/`: depends on `can/` and `isotp/`, and must not
  depend on `elm/`.
- Functional versus physical request behavior.
- Multiple responder collection, response limits, filters, and receive address.
- CAN-only automatic search order `6 -> 7 -> 8 -> 9`.
- Mapping transport results to ELM-level outcomes without formatting text here.

## Steps

1. Define a `DiagnosticRequest` from session state and parsed input.
2. Route it through ISO-TP TX then create a receive state per accepted source.
3. Enforce the responder cap and requested response count.
4. Implement auto-search as an explicit sequence of configurations, recording
   the successful protocol only after a complete response.
5. Return responders, raw frames, and typed failure details to the ELM layer.

## Acceptance criteria

- A functional request can return two distinct source responses.
- A physical request stops at its first complete matching response.
- No-data, bus error, and auto-search exhaustion are distinct results.
- The service does not format ELM text or depend on Bluetooth.

## Tests

- One/two responder scenarios, filtering, and explicit response count.
- 11-bit and 29-bit auto-search success and exhaustion.
- Receive timeout after TX and a transmit failure before RX.
