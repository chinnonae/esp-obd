# D04 - Signal Decoder and Synthetics

**Status:** Done (2026-09-05)
**Depends on:** none (pure functions, no transport/storage)

## Goal

Correctly decode OBDb `fmt`-described signals from raw response bytes, and
compute `synthetics` (`ratio`) derived values — verified against fixtures,
independent of any real hardware.

## Background

Schema details (from [PLAN.md](../PLAN.md), cross-checked against real
`OBDb/SAEJ1979` and `OBDb/Nissan-Leaf` files):

- Request bytes = concatenation of the `cmd` object's key+value, e.g.
  `{"01":"0C"}` → `010C`.
- `fmt.bix` = starting bit offset into the response *data* bytes (i.e. after
  the `41 <pid>` / `62 <pid>` echo bytes are stripped) — defaults to `0` if
  absent. `fmt.len` = bit length. Optional `blsb` (byte-swapped), `sign`,
  `mul`/`div`/`add` for scaling, or `map` (integer → human string, mutually
  exclusive with `max`/`unit`-based numeric scaling per the schema's
  `anyOf`).
- `synthetics[].formula` currently only defines `op: "ratio"`, computing
  `a / b` from two already-decoded signal ids.

## Scope

- `dashboard/js/decoder.js`:
  - `decodeResponse(command, rawHexBytes)` — given one `commands[]` entry
    and the raw response bytes (already stripped of the `41 <pid>`/`62
    <pid>` prefix by the caller), returns `{signalId: value}` for every
    signal in `command.signals`.
  - `extractBits(bytes, bix, len, {blsb, sign})` — the core bit-extraction
    primitive.
  - `applyScale(rawValue, fmt)` — applies `mul`/`div`/`add`, or looks up
    `map`, returning the final scaled value (or the mapped string).
  - `computeSynthetics(signalValues, synthetics)` — given the full set of
    currently-known `{id: value}` pairs and a profile's `synthetics` array,
    returns any newly-computable derived `{id: value}` pairs (skips any
    whose `a`/`b` inputs aren't present yet).

## Steps

1. Implement `extractBits` first, independent of the rest — this is the
   part most likely to have off-by-one bit/byte-order bugs.
2. Implement `applyScale` and `decodeResponse` on top of it.
3. Implement `computeSynthetics` (`ratio` only, per current schema).
4. Write `dashboard/test/decoder-test.html` (see Verification) with fixture
   cases pulled directly from real OBDb signalsets, e.g.:
   - RPM: `{"len": 16, "max": 16383.75, "div": 4, "unit": "rpm"}` against
     raw bytes `1A F8` → expect `1726` (per the SAEJ1979 file's own
     example shape — recompute the exact expected value from the formula
     when writing the test, don't hand-wave it).
   - A `sign: true` field from a real signalset (e.g. Nissan-Leaf's current
     signals) to confirm negative values decode correctly.
   - A `map`-based enum field.
   - A `ratio` synthetic once its two inputs are present, and confirm it's
     absent from the output when only one input is present.

## Acceptance criteria

- All fixture cases in `decoder-test.html` pass when opened in a browser.
- `extractBits` handles `bix` values that don't fall on byte boundaries
  (e.g. `bix: 9, len: 1` — the DTC-readiness bits from SAEJ1979's own
  signalset) correctly.
- `computeSynthetics` never throws on missing inputs — it just omits that
  synthetic from its result.

## Verification

No Node/npm toolchain is available in this dev environment (checked:
`node`/`npm` are not on `PATH`), so this task's "tests" are a static,
dependency-free HTML page (`dashboard/test/decoder-test.html`) that imports
`decoder.js` as an ES module, runs the fixture assertions, and renders
pass/fail to the page. Open it via the Browser tool or any static server and
confirm all cases show green. This stands in for the automated unit-test
step other tasks would normally have.

## Notes

- Keep this module free of any DOM/localStorage/serial dependency so the
  test page can import it in isolation.
- Verified 2026-09-05: `dashboard/test/decoder-test.html` (10 fixture
  cases pulled from real `OBDb/SAEJ1979` field shapes, plus a self-defined
  `blsb` case) all pass when served locally and opened in the integrated
  browser.
