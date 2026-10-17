# ESP-OBD Android app

Planned Flutter app for the ESP-OBD adapter. Nothing is implemented yet; this
folder reserves its place in the monorepo.

## Planned starting point

```powershell
cd apps/android
flutter create --org <your.reverse.domain> --project-name esp_obd --platforms android .
```

Building needs the Flutter SDK and the Android SDK (Android Studio installs
the latter); neither is required for the firmware or the web dashboard.

## Talking to the adapter

The adapter is a Bluetooth **Classic** SPP device named `ESP-OBD` (PIN
`5678`) speaking the ELM327 command set, so the app connects the same way
Car Scanner does. Flutter's common Bluetooth packages are BLE-only; Classic
SPP needs a plugin that supports RFCOMM sockets on Android.

The firmware's command contract is
[firmware/docs/ELM_COMMAND_BEHAVIOR.md](../../firmware/docs/ELM_COMMAND_BEHAVIOR.md),
and a known-good initialisation sequence (Car Scanner's, replayed in the
firmware tests) is in
[firmware/test/unit/test_elm_bluetooth_session/test_main.cpp](../../firmware/test/unit/test_elm_bluetooth_session/test_main.cpp)
(`test_car_scanner_init_sequence_matches_reference_adapter`).
