# T09 - CAN Command Families and Monitoring

**Status:** Planned

## Goal

Complete the in-scope CAN-specific ELM behavior on top of the stable core.

## Scope

- `ATSP`, `ATTP`, `ATDP`, `ATDPN`, `ATST`, `ATAT`, `ATCTM`, and `ATPC`.
- `ATSH`, `ATCP`, `ATCRA`, `ATAR`, `ATCF`, `ATCM`, `ATCAF`, `ATCFC`, and
  manual flow-control settings.
- Extended addressing, monitoring (`ATMA`, `ATMR`, `ATMT`), silent monitor,
  CAN status, RTR, variable DLC, and buffer dump.
- The explicitly unsupported-command parameterized test set.

## Steps

1. Implement commands one family at a time, beginning with protocol and header
   state, then filters, then flow control, then monitoring.
2. Add a contract test before each handler is implemented.
3. Keep monitor filtering in portable code and keep the TWAI adapter unaware of
   ELM command strings.
4. Implement `ATCS` from a typed `CanStatus` rather than raw driver output.
5. Verify every unsupported command returns `?` without state mutation or TX.

## Acceptance criteria

- Every in-scope command row in the behavior contract has a passing contract
  test.
- A monitor session prints only matching frames and always exits safely.
- Manual/automatic flow-control modes produce exact expected frames.
- `ATRV` and `ATIGN` are unsupported, never fabricated.

## Tests

- Parameterized syntax/side-effect tests for command families.
- Filter and monitor capture tests for standard and extended IDs.
- Unsupported commands assert unchanged session plus empty TX capture.
