# ESP-OBD Reimplementation Plan

## Overview

Redesign ESP-OBD for simplicity and testability:
- **ESP32 firmware**: WiFi-based WebSocket server with hardcoded CAN config (500kbps, 29-bit)
- **Web dashboard**: GitHub Pages hosted static app that connects to ESP32 and displays live data
- **Serial debug interface**: Interactive UART0 commands for testing and IP discovery
- **Testing**: Unit tests for core logic, integration tests on device

## Architecture

### ESP32 Firmware

**Role**: Real-time data collector and server
- CAN bus listener (hardcoded 500kbps, 29-bit addressing)
- WiFi connection (SSID/password hardcoded, no mDNS)
- WebSocket server for client connections
- Local polling engine collecting OBD data
- Serial debug interface on UART0 for testing and IP discovery

**Key modules**:
```
src/
├── main.cpp                 # Entry point, WiFi/WebSocket setup
├── can/
│   ├── can_driver.h/cpp     # CAN interface to ESP32 TWAI
│   └── can_frame.h          # CAN frame structure (fixed 29-bit)
├── obd/
│   ├── pid_decoder.h/cpp    # Parse CAN frames to OBD values
│   └── polling_engine.h/cpp # Poll cycle logic (timing, PIDs)
├── transport/
│   ├── websocket_server.h/cpp # WebSocket protocol handler
│   ├── wifi_config.h/cpp    # WiFi SSID/password, connection
│   └── serial_debug.h/cpp   # Interactive UART0 commands
└── utils/
    └── json_builder.h/cpp   # Build JSON telemetry packets
```

**WebSocket Protocol** (ESP32 → Web):
```json
{
  "type": "telemetry",
  "timestamp": 1234567890,
  "values": {
    "rpm": 2500,
    "speed": 45,
    "throttle": 25,
    "coolant_temp": 90,
    "intake_air_temp": 35,
    "o2_sensor": 0.8
  }
}
```

### Web Dashboard

**Role**: Display and interact with ESP32
- Hosted on GitHub Pages (static HTML/CSS/JS)
- WebSocket client connecting to ESP32 IP (entered by user)
- Real-time gauge/chart display
- No build step required (plain JavaScript)

**Key files**:
```
dashboard/
├── index.html              # Main page + gauges
├── js/
│   ├── websocket-client.js # Connect to ESP32, parse telemetry
│   ├── display.js          # Update gauges/charts
│   └── config.js           # Store ESP32 IP locally
├── css/
│   └── app.css             # Styling for gauges
└── manifest.webmanifest    # PWA metadata
```

**User Flow**:
1. Power on ESP32 (connect to development PC via USB)
2. Open serial monitor: `pio device monitor -e ioxesp32`
3. Type `STATUS` to see device IP address
4. Open web dashboard on mobile (GitHub Pages URL)
5. Enter device IP address: `192.168.1.100`
6. Click "Connect"
7. Live telemetry displayed in gauges

## Testing Strategy

### Phase 1: Unit Tests (Hardware-free)
Test core logic without ESP32 or hardware:
- CAN frame parsing (known inputs → expected OBD values)
- Polling engine state machine (timing, PID rotation)
- JSON builder (OBD values → WebSocket packet)

**Run**: `pio test -e native_test`

### Phase 2: Integration Tests (ESP32 connected)
Test end-to-end on real hardware:
- ESP32 firmware builds and boots
- CAN bus listener receives frames
- WebSocket server accepts connections
- Telemetry correctly formatted and sent
- Web app connects and displays data

**Run**: Manual (upload to ESP32, test with web app)

### Phase 3: Hardware Validation
- Real CAN traffic from OBD-II scanner or vehicle
- Accurate PID decoding
- WebSocket connection stability
- Web app rendering under load

## Implementation Roadmap

### Task 1: Core CAN & OBD Module
- Implement CAN frame structure and parser
- Implement OBD PID decoder (hardcoded SAE J1979 PID set)
- Unit tests for frame parsing

### Task 2: Polling Engine
- Implement polling loop (cyclic PID requests)
- Implement state machine for transmission
- Handle ECU responses and timeouts
- Unit tests for timing and sequencing

### Task 3: ESP32 Platform Layer & Serial Debug
- Initialize TWAI CAN controller (500kbps, 29-bit)
- Implement CAN transmit/receive
- Initialize WiFi (no mDNS)
- Implement serial debug interface (STATUS, TEST_PID, DISCOVER, REBOOT commands)
- Serial debug logging (print IP address at startup)

### Task 4: WebSocket Server
- Implement minimal WebSocket protocol (RFC 6455)
- JSON telemetry packet builder
- Connection state management
- Broadcast to all connected clients

### Task 5: Web Dashboard
- Minimal HTML page with gauges
- WebSocket client (connect, parse, display)
- Local IP/connection storage
- Mobile-friendly layout

### Task 6: Integration & Testing
- Build and upload to ESP32
- Connect web app and verify telemetry flow
- Stress testing and optimization
- Documentation

## Hardcoded Configuration

All configuration is baked into firmware and web app:

**CAN Settings** (in `include/can/can_config.h`):
```cpp
static constexpr uint32_t CAN_BITRATE = 500000;
static constexpr bool USE_29BIT_ID = true;  // Extended addressing
```

**WiFi Settings** (in `include/transport/wifi_config.h`):
```cpp
static constexpr const char* WIFI_SSID = "MyNetwork";
static constexpr const char* WIFI_PASSWORD = "password";
static constexpr uint16_t WEBSOCKET_PORT = 8080;
```

**OBD PIDs** (in `include/obd/pid_config.h`):
```cpp
// Hardcoded list of PIDs to poll
// RPM, Speed, Throttle, Temperatures, O2, etc.
```

## Benefits

1. **Simplicity**: No ELM327 command layer, no Bluetooth hassle
2. **Testability**:
   - Core logic testable without hardware
   - WebSocket protocol is standard and well-understood
   - Web app is standard JavaScript (no special toolchain)
   - Serial debug interface for testing individual PIDs and discovering supported PIDs
3. **Reliability**:
   - IP connection more stable than Bluetooth Serial
   - Server-side polling ensures consistent data collection
   - Serial debug interface provides real-time status and error visibility
4. **Scalability**:
   - Can add more clients (multiple phones viewing same data)
   - Can extend PID list without firmware rebuild (future: SD card config)
5. **Mobile-friendly**: Standard web tech, works on any phone with a browser

## Next Steps

1. Create `include/can/can_config.h` with CAN constants
2. Create `include/obd/pid_config.h` with SAE J1979 PID list
3. Implement CAN frame parser and unit tests
4. Implement polling engine and unit tests
5. Implement ESP32 main loop and WebSocket server
6. Create web dashboard on GitHub Pages
7. Integration testing and refinement
