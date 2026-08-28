# ESP-OBD

A fresh ESP32 starting point for the future CAN OBD-II adapter.

The current firmware is deliberately minimal:

- prints an identity banner and `Hello, world!` once on UART0 at 115200 baud;
- blinks the status LED every 500 ms.

## Hardware and safety

Confirmed values live in [include/core/hardware_constants.h](include/core/hardware_constants.h)
and [include/core/build_info.h](include/core/build_info.h); this section only
summarizes them.

- Target board: `ioxesp32` (see `platformio.ini`).
- CAN: ESP32 TWAI peripheral on GPIO 26 (TX) / GPIO 27 (RX), through a
  **Classical CAN** transceiver only. No CAN FD.
- Status LED: the board's built-in LED (GPIO 5), active-high.
- Bluetooth: Classic SPP, advertised as `ESP-OBD`, fixed pairing PIN `5678`.
  This is the only ELM327 command channel.
- UART0 is a separate debug console. Its output is always prefixed `#DBG:`
  and is never sent to Bluetooth.

This board and firmware do **not** support J1850, ISO 9141/KWP electrical
interfaces, battery-voltage measurement, or ignition sensing — there is no
wiring for any of them. Any document or reply claiming otherwise is wrong.

> [!WARNING]
> This adapter can transmit diagnostic traffic, including services that can
> write to or reconfigure an ECU. There is no firmware-level whitelist or
> unlock gate on requests — see the write/control request policy in
> [T00's decision log](docs/tasks/00-project-baseline.md#decision-log). Any
> well-formed request you or a connected app sends is transmitted as-is,
> matching real ELM327 behavior. Test on a stationary vehicle or a CAN
> simulator until you trust the app and requests you are sending.

## Build and upload

```powershell
pio run
pio run --target upload
pio device monitor --baud 115200
```

The preceding ELM/CAN implementation was saved in Git commit `f5c36ed`
(`legacy: checkpoint ELM CAN adapter implementation`).

## Testing

Portable code has a fast, hardware-free test suite (no ESP32 board needed):

```powershell
pio test -e native_test
```

This does not require a separately installed gcc/g++ — it builds with the
`zig` compiler already bundled in `.venv` (see
[test/native_toolchain.py](test/native_toolchain.py)). See
[test/README.md](test/README.md) for suite layout and shared fakes.

## Reimplementation plan

The dependency-ordered task board is in [docs/tasks/INDEX.md](docs/tasks/INDEX.md).
Each task has its own scope, acceptance criteria, and test plan.
