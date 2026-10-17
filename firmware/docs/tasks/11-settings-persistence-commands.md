# T11 - Settings Persistence Commands

**Status:** Done (started 2026-08-28, completed 2026-08-28)

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
  `ATRD`, `ATSDhh`. Not `AT@1`: that's a fixed, stateless string handled by
  [T03](03-elm-core-and-formatting.md) alongside `ATI`.
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

## Notes

- `PersistedSettingsCache` ([include/elm/persisted_settings.h](../../include/elm/persisted_settings.h))
  is deliberately a separate struct from `ElmSession`, not new fields added
  to it: `ATZ`/`ATD`/`ATWS`'s `session.resetToDefaults()` does
  `*this = ElmSession{}`, which would silently wipe the device id/saved
  byte/`M` state on every reset if they lived there -- defeating the point
  of persistence. `ElmCommandEngine` now holds both, loading the cache from
  the store once at construction.
- `ISettingsStore` ([include/core/i_settings_store.h](../../include/core/i_settings_store.h))
  lives under `core/`, not `elm/`: it has no ELM-specific meaning, so `elm/`
  depends on the interface the same way it already depends on `can/`
  types. The in-memory fake (`test/support/in_memory_settings_store.h`)
  stays test-only, matching this task's own framing -- it is never linked
  into the firmware build.
- `ElmCommandEngine`'s constructor now requires an `ISettingsStore&` (no
  default), so every existing T03 test in `test/contract/
  test_core_session_commands/` was updated to construct an
  `InMemorySettingsStore` alongside the engine. This was judged worth the
  churn over giving `ElmCommandEngine` a private owned fake store, which
  would have put test-only code in a class that also runs on the target.
- `M` (`ATM0`/`ATM1`) is session-level, not itself persisted: a fresh
  engine always loads with persistence enabled, regardless of what `M` was
  set to before. Only `AT@2`/`AT@3`/`ATRD`/`ATSDhh`'s *values* persist.
- `dispatchSettingsCommand` is wired into `ElmCommandEngine::execute()`
  right after `dispatchCoreCommand` in the same `AT` dispatch chain (T03's
  `?` fallback still catches anything neither recognizes) -- not left as a
  standalone module for a later task to integrate.
- `appendHexByte` was promoted from `elm_formatter.cpp`'s anonymous
  namespace to a declared function in `elm_formatter.h`, so `ATRD`'s
  two-hex-digit rendering reuses it instead of duplicating the hex-digit
  table.
- New suite `test/contract/test_settings_persistence_commands/`: `AT@2`
  default and round-trip, `AT@3` length/hex validation, `ATRD`/`ATSDhh`
  round-trip and validation, `M0` blocking a store write while still
  applying in-session (verified against a *second, fresh* engine over the
  same store standing in for a reconnect), `M1` persisting by default and
  after a prior `M0`, and `ATFE` restoring defaults both immediately and
  in the store.

Verified: `pio test -e native_test` (95/95 passing across nine suites) and
`pio run -e ioxesp32` still builds.
