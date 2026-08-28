# T07 - ESP32 Platform Adapters

**Status:** Planned

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
