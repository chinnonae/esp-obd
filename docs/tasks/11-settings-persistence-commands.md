# T11 - Settings Persistence Commands

**Status:** Planned

## Goal

Implement the AT commands that read, write, and gate persisted adapter
settings, and pin down exactly which fields are persisted and when a change
takes effect — entirely as portable code, with no ESP32 dependency.

## Scope

- `ISettingsStore` interface and an in-memory fake implementation, used only
  by native tests; this is a software test double, not a description of the
  real storage. On the target device, T07's `Esp32SettingsStore` implements
  this same interface against ESP32 NVS (flash already built into the chip;
  no additional hardware).
- Command handlers for `AT@2`, `AT@3hhhhhhhhhhhh`, `ATM0`/`ATM1`, `ATFE`,
  `ATRD`, `ATSDhh`.
- The exact persisted-field list: the 12-hex-digit device identifier, one
  saved data byte, and the `M0`/`M1` persistence-enabled flag itself.
- Validation rules: reject malformed input without touching stored state;
  `M0` blocks *future* setting changes from reaching the store without
  discarding what is already stored.
- `ATFE` semantics: erase all persisted fields and restore in-memory defaults
  without requiring a power cycle to observe the reset.

## Steps

1. Define `ISettingsStore` (read/write per field) and an in-memory fake
   implementing it, alongside the fakes from
   [T01](01-native-test-foundation.md).
2. Extend the `ElmSession`/action types from
   [T03](03-elm-core-and-formatting.md) so a command handler can request a
   settings-store read/write without touching `ICanPort`.
3. Implement `AT@2`/`AT@3` against the interface, including validation of
   exactly 12 hex digits.
4. Implement `ATM0`/`ATM1`, and gate every other persisting handler on the
   current `M` state.
5. Implement `ATRD`/`ATSDhh` for the single saved data byte.
6. Implement `ATFE` to clear every persisted field defined above.
7. Add contract tests for valid/invalid input, `M0` gating, and post-`ATFE`
   defaults, all against the in-memory fake.

## Acceptance criteria

- Every persistence row in the behavior contract (§2.1: `AT@2`, `AT@3`,
  `ATM0`/`ATM1`, `ATFE`, `ATRD`, `ATSDhh`) has a passing contract test.
- Invalid input never mutates a stored field.
- `M0` demonstrably prevents a later valid setting change from reaching the
  store, while still letting that change take effect for the current session.
- These handlers and the fake store compile and run in `native_test` with no
  ESP32 dependency; `ISettingsStore` has exactly one abstract shape that T07
  later implements against real NVS.

## Tests

- Valid/invalid `AT@3` payloads, including boundary lengths around 12 hex
  digits.
- `ATSDhh` valid/invalid values and `ATRD` round-trip.
- `M0`, then a setting change, then `ATRD`/`AT@2` shows the old persisted
  value; `M1` shows the new one.
- `ATFE` followed by a fresh session read returns factory defaults for every
  persisted field.
