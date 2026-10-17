# T07 - ESP32 Platform Adapters

**Status:** Blocked (started 2026-08-28) -- code complete; basic hardware
operation confirmed on a real board and vehicle 2026-08-28 (see **Hardware
validation update**), but the formal bench checklist (explicit bitrate
switch, listen-only ack-suppression) is still not done. See **Blocker**.

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

## Hardware validation update (2026-08-28)

A real ESP32 board (`ioxesp32`, USB-serial via a Silicon Labs CP210x
bridge) was connected and flashed this session, with a phone scanner app
against a live vehicle. This is genuine evidence, not a bench checklist
substitute -- see what's still missing below.

Confirmed working:
- TWAI at 500 kbit/s talking to a real vehicle: a functional (`0100`)
  broadcast got responses from **six distinct ECUs**, and later requests
  (Mode 01 PIDs, VIN via Mode 09) returned real, correctly-decoded vehicle
  data -- including a complete 17-character VIN across a First Frame + 2
  Consecutive Frames.
- Bluetooth Classic SPP: paired and exchanged a full real ELM327
  conversation (`ATZ`, `ATE0`, `ATSP7`, `ATH1`, Mode 01/09 requests) with a
  car-scanner app.
- `Esp32SettingsStore`/NVS: no longer crashes or silently fails (see the
  two bugs fixed below); persistence itself (write survives a reboot)
  wasn't separately exercised this session.

Two real bugs were found and fixed only because of this hardware run (see
commits from 2026-08-28):
1. `Esp32BluetoothTransport::begin()` called `setPin()` before `begin()`;
   `BluetoothSerial::setPin()` requires the stack already started
   (confirmed via the exact log line: `BT is not initialized. Call
   begin() first`). Fixed by swapping the order.
2. `Esp32SettingsStore` read NVS from its constructor, but it's a
   file-scope global -- its constructor runs during C++ static
   initialization, before `setup()` ever calls `nvs_flash_init()`
   (confirmed via `nvs_open failed: NOT_INITIALIZED`). Fixed by moving
   the NVS read into an explicit `load()`, and restructuring `main.cpp`
   to construct `ElmApplication`/`Esp32BluetoothTransport`/
   `Esp32UartDebugSink` inside `setup()`, after `nvs_flash_init()` +
   `load()`, instead of as file-scope globals.

A third, non-hardware-specific bug was also caught this way: `ATH1`
(headers on) only ever rendered a responder's *first* raw frame, so a
multi-frame VIN response was truncated to ~3-6 characters in the scanner
app. Fixed in `ElmApplication::formatDiagnosticResult()` to walk every raw
frame per responder (First Frame + each Consecutive Frame), each correctly
trimmed for CAN-level padding under `ATD0`.

## Blocker

Still not done, and must not be claimed as done:

- "The target build initializes and changes 250/500 kbit/s correctly" --
  only 500 kbit/s was exercised (against a real vehicle); switching to
  250 kbit/s specifically was not tested.
- "A benchmark smoke test demonstrates normal and listen-only controller
  modes" -- listen-only (`ATCSM1`) was not exercised against real hardware;
  only normal mode was used.
- The Bench NVS test (a value survives a power cycle) -- not separately
  checked.

Do not mark this task `[x]` until someone runs the remaining bench
checklist items above; record the dated result here when that happens.

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
