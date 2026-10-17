# ESP-OBD

An ESP32 Bluetooth Classic ELM327-compatible OBD-II adapter, and the apps
that talk to it.

| Folder | What it is | Stack |
|---|---|---|
| [firmware/](firmware/README.md) | Adapter firmware for the ESP-OBD2 board: ELM327 command set over Bluetooth Classic SPP, Classical CAN via the ESP32 TWAI peripheral. | PlatformIO, Arduino, C++17 |
| [apps/dashboard/](apps/dashboard/README.md) | Web dashboard (PWA), deployed to GitHub Pages. | HTML, JavaScript |
| [apps/android/](apps/android/README.md) | Android app (planned). | Flutter |

The dashboard's vehicle profiles in `apps/dashboard/data/` are generated
from [OBDb](https://github.com/OBDb) signal sets (Honda-Civic, SAEJ1979); see
the `*.ATTRIBUTION.md` files there. The OBDb repositories are not vendored:
some of their test-case filenames contain `|`, which Windows cannot check
out. Clone them separately (a sparse checkout of `signalsets/` works on
Windows) when the data needs regenerating.

## Firmware quick start

Use PlatformIO IDE in VS Code (see [Editor setup](#editor-setup)); Build and
Upload target the board. The hardware-free native tests need a one-time
setup of `firmware/.venv`, described in
[firmware/README.md](firmware/README.md#testing). From a terminal:

```powershell
cd firmware
pio test -e native_test     # hardware-free tests
pio run -t upload           # build and flash the board
```

See [firmware/README.md](firmware/README.md) for hardware, safety notes and
the full command reference in
[firmware/docs/ELM_COMMAND_BEHAVIOR.md](firmware/docs/ELM_COMMAND_BEHAVIOR.md).

## Editor setup

Open [esp-obd.code-workspace](esp-obd.code-workspace) in VS Code
(**File > Open Workspace from File...**). It lists `firmware/`,
`apps/dashboard/` and `apps/android/` as separate workspace folders, which is
what lets the PlatformIO extension find `firmware/platformio.ini`: it only
detects projects at the root of a workspace folder. Opening just `firmware/`
also works.
