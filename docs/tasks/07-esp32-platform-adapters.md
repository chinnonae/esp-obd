# T07 - ESP32 Platform Adapters

**Status:** Blocked (started 2026-08-28) -- code complete and building; needs
a physical bench to verify TWAI/NVS behavior. See **Blocker** below.

## Goal

Connect the portable diagnostic core to the ESP32 without letting ESP-IDF types
or lifecycle rules spread through the program.

## Scope

- `Esp32TwaiCanPort` for configure, send, receive, status, normal, and
  listen-only modes.
- `Esp32Clock` based on monotonic milliseconds.
- `Esp32SettingsStore`, implementing the `ISettingsStore` interface from
  [T11](11-settings-persistence-commands.md) against ESP32 NVS. This task
  does not define that interface or its fields; it only backs it with real
  flash storage.
- Error translation and a minimal application composition root.

## Steps

1. Implement only the adapter conversion between `CanFrame` and
   `twai_message_t` in `platform/esp32/`. `receive()` must call
   `twai_receive()` with a zero wait and return immediately; it must never
   block the caller, per [Architecture](../ARCHITECTURE.md)'s `ICanPort`
   contract.
2. Make bitrate/mode changes safely stop and restart TWAI as required.
3. Translate ESP-IDF failures into portable typed results and log only on UART0.
4. Implement `Esp32SettingsStore` against the `ISettingsStore` interface from
   T11, backed by ESP32 NVS.
5. Build a small `ElmApplication` that receives adapters by constructor.

## Acceptance criteria

- Portable headers compile in `native_test` without ESP32 headers.
- The target build initializes and changes 250/500 kbit/s correctly.
- A benchmark smoke test demonstrates normal and listen-only controller modes.
- Platform errors do not dictate user-visible ELM text.

## Tests

- Target build for `ioxesp32`.
- Bench CAN test: send one known frame, receive one known frame, and verify
  listen-only mode does not transmit acknowledgements.
- Bench NVS test: a value written through `Esp32SettingsStore` survives a
  power cycle. Native fake-store behavior is covered in T11, not here.

## Blocker

This session has no physical ESP32 board or CAN bench attached, so the two
genuinely hardware-dependent acceptance criteria are unverified:

- "The target build initializes and changes 250/500 kbit/s correctly."
- "A benchmark smoke test demonstrates normal and listen-only controller
  modes," and the Bench CAN / Bench NVS tests above.

Everything checkable without hardware is done and verified (see Notes).
Do not mark this task `[x]` until someone runs the bench checklist above
on real hardware; record the result here when that happens.

## Notes

- `Esp32TwaiCanPort` ([include/platform/esp32/esp32_twai_can_port.h](../../include/platform/esp32/esp32_twai_can_port.h))
  implements `ICanPort` against the ESP-IDF TWAI driver (verified against
  the actual bundled headers in `.platformio/packages/framework-
  arduinoespressif32/tools/sdk/esp32/include/driver/include/driver/
  twai.h`, not guessed): `configure()` stops/uninstalls before
  reinstalling so a bitrate/mode change is safe to call more than once;
  `receive()` calls `twai_receive()` with `ticks_to_wait = 0`, never
  blocking, per the `ICanPort` contract; ESP-IDF `esp_err_t` values map to
  `CanResult::{Ok,Timeout,BusError}` and never reach ELM text directly.
- `Esp32Clock` wraps Arduino `millis()`.
- `Esp32SettingsStore` implements T11's `ISettingsStore` against ESP32 NVS
  via the bundled `Preferences` library (confirmed present under the
  framework's `libraries/` dir). Arduino `String` is used only inside this
  one `.cpp`, immediately copied into a plain `char[13]` before returning
  -- it never crosses the `ISettingsStore` interface.
- `include/app/elm_application.h`: a deliberately thin composition root
  (header-only, no `.cpp`) that owns the injected `ICanPort&` and an
  `ElmCommandEngine`, and can `execute()` AT commands end to end. It does
  *not* yet turn a `DiagnosticRequest`-kind `ElmReply` into an actual
  `DiagnosticTransport` transaction -- that requires a real transport to
  receive the eventual async reply, which is T08's job, not invented here.
  `poll(now)` exists (per the documented `ElmApplication` contract in
  [ARCHITECTURE.md](../ARCHITECTURE.md)) but is currently a no-op; T08
  extends it.
- `app/` stayed portable (no ESP32 headers), so it's tested in
  `native_test` (new suite `test/unit/test_elm_application/`) using
  `FakeCanPort`/`InMemorySettingsStore` -- construction from adapters, AT
  command execution, and a settings command reaching the injected store.
  `platform/esp32/` itself has no native tests (impossible without ESP-IDF
  headers); its only available verification here is `pio run -e ioxesp32`.

Verified: `pio test -e native_test` (98/98 passing across ten suites,
including the new `test_elm_application`) and `pio run -e ioxesp32` builds
clean with all three adapters compiled and linked (confirmed by inspecting
the build log, not just the exit code).
