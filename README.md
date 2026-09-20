# ESP-OBD Hello World

A simple hello world application for the ESP32 using PlatformIO.

## Prerequisites

- PlatformIO CLI installed
- Python environment set up (virtual environment in `.venv`)

## Building

```bash
pio build -e ioxesp32
```

## Uploading

```bash
pio run -e ioxesp32 -t upload
```

## Monitoring

```bash
pio device monitor -e ioxesp32
```

You should see "Hello, World!" printed to the serial console.

## Project Structure

- `platformio.ini` - PlatformIO configuration
- `src/main.cpp` - Main application code

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
