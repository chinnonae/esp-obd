# ESP-OBD Architecture

## System Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                         Mobile Phone                         │
│  ┌───────────────────────────────────────────────────────┐  │
│  │         Web Dashboard (GitHub Pages)                  │  │
│  │                                                       │  │
│  │  ┌────────────────────────────────────────────────┐  │  │
│  │  │  User Interface (Gauges, Charts)               │  │  │
│  │  └─────────────────┬──────────────────────────────┘  │  │
│  │                    │                                 │  │
│  │  ┌────────────────▼───────────────────────────────┐  │  │
│  │  │  WebSocket Client (JS)                         │  │  │
│  │  │  - Connect to ESP32 IP:8080                    │  │  │
│  │  │  - Parse JSON telemetry                        │  │  │
│  │  │  - Update display in real-time                 │  │  │
│  │  └──────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────┘  │
└──────────────────────────┬─────────────────────────────────┘
                           │
                     WiFi (IP Network)
                           │
┌──────────────────────────▼─────────────────────────────────┐
│                     ESP32 Device                            │
│  ┌───────────────────────────────────────────────────────┐ │
│  │              ESP32 Main Application                   │ │
│  │                                                       │ │
│  │  ┌─────────────────────────────────────────────────┐ │ │
│  │  │  WebSocket Server                               │ │ │
│  │  │  - Listen on :8080                              │ │ │
│  │  │  - Broadcast telemetry packets                  │ │ │
│  │  │  - Serialize OBD values to JSON                 │ │ │
│  │  └────────────────────┬────────────────────────────┘ │ │
│  │                       │                              │ │
│  │  ┌────────────────────▼────────────────────────────┐ │ │
│  │  │  Polling Engine                                 │ │ │
│  │  │  - Maintains polling cycle state                │ │ │
│  │  │  - Generates CAN transmission requests          │ │ │
│  │  │  - Collects responses from CAN                  │ │ │
│  │  │  - Builds OBD value map                         │ │ │
│  │  └────────────────────┬────────────────────────────┘ │ │
│  │                       │                              │ │
│  │  ┌────────────────────▼────────────────────────────┐ │ │
│  │  │  CAN Bus Driver                                 │ │ │
│  │  │  - Send/receive CAN frames                      │ │ │
│  │  │  - Parse 29-bit extended IDs                    │ │ │
│  │  │  - 500kbps hardcoded                            │ │ │
│  │  │  - Uses ESP32 TWAI peripheral                   │ │ │
│  │  └──────────────────┬───────────────────────────────┘ │ │
│  │                     │                                 │ │
│  │  ┌──────────────────▼────────────────────────────┐   │ │
│  │  │  WiFi & Network                               │   │ │
│  │  │  - Connect to SSID (hardcoded)                 │   │ │
│  │  │  - mDNS discovery (esp-obd.local)             │   │ │
│  │  │  - Maintain WebSocket connections             │   │ │
│  │  └────────────────────────────────────────────────┘   │ │
│  └───────────────────────────────────────────────────────┘ │
│                                                             │
│  ┌───────────────────────────────────────────────────────┐ │
│  │              Hardware Interfaces                      │ │
│  │                                                       │ │
│  │  CAN Bus ─────► ESP32 TWAI (GPIO 26/27, 500kbps)   │ │
│  │  Serial (UART0) ─► Debug/Monitor Output             │ │
│  │  WiFi Antenna ─── WiFi Connection (2.4GHz)         │ │
│  └───────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────┘
         │
         │ CAN Bus (500kbps, 29-bit ID)
         │
    ┌────▼─────┐
    │ OBD-II   │
    │ Vehicle  │
    └──────────┘
```

## Component Interaction Flow

### Startup Sequence

```
1. ESP32 Powers On
   └─► Initialize Serial (UART0, 115200 baud)
   └─► Initialize CAN (TWAI, 500kbps, 29-bit)
   └─► Connect WiFi (SSID/password hardcoded)
   └─► Register mDNS hostname (esp-obd.local)
   └─► Start WebSocket Server (port 8080)
   └─► Begin Polling Engine

2. Web Dashboard Opens
   └─► Display IP address input field
   └─► User enters "esp-obd.local" or finds via mDNS

3. User Clicks "Connect"
   └─► WebSocket client connects to ws://esp-obd.local:8080
   └─► Server sends initial state
   └─► Web app enters "connected" state
```

### Data Collection Cycle (every 100ms)

```
Polling Engine Loop:
├─ Check which PID to request next (rotates through hardcoded list)
├─ Build CAN Frame:
│  ├─ ID: 0x7DF (CAN Request)
│  ├─ DLC: 8 bytes
│  ├─ Data: 02 10 xx 00 00 00 00 00 (PID request)
│  └─ Send via CAN driver
│
├─ Wait for Response (1000ms timeout)
│  └─ CAN driver receives frames, stores in queue
│
├─ Parse Response (CAN ID 0x7E8, multiframe if needed)
│  ├─ Extract PID value
│  ├─ Store in value_map[pid_id] = value
│  └─ On timeout, mark as stale
│
└─ Every 10 iterations (1 second):
   ├─ Build JSON telemetry packet
   ├─ Send via WebSocket to all connected clients
   └─ Clients update UI gauges
```

### WebSocket Telemetry Packet

```json
{
  "type": "telemetry",
  "timestamp": 1695148532,
  "poll_cycle": 24,
  "values": {
    "rpm": 2500,
    "speed": 65,
    "fuel_pressure": 55,
    "intake_air_temp": 28,
    "coolant_temp": 92,
    "throttle_position": 18,
    "o2_sensor_1": 0.78,
    "fuel_status": 2,
    "num_trouble_codes": 0
  },
  "status": {
    "connected": true,
    "can_frames_received": 1542,
    "websocket_clients": 1,
    "uptime_seconds": 3600
  }
}
```

## Module Responsibilities

### `src/main.cpp`
- ESP32 entry point (`setup()`, `loop()`)
- Initialize all subsystems in correct order
- Main loop: service CAN, polling, WebSocket, WiFi

### `can/can_driver.h/cpp`
- **Responsibility**: Abstraction over ESP32 TWAI hardware
- **Public API**:
  - `init(bitrate, use_29bit)` → Initialize controller
  - `send(frame)` → Send a CAN frame
  - `receive()` → Get pending frame (non-blocking)
  - `is_initialized()` → State check
- **Tests**: Unit tests with mock CAN frames

### `obd/polling_engine.h/cpp`
- **Responsibility**: Stateful polling state machine
- **Public API**:
  - `init(pid_list)` → Initialize with PID set
  - `tick(can_driver, now_ms)` → Advance state, transmit/receive
  - `get_values()` → Current parsed OBD values
  - `is_cycling()` → Currently waiting for response?
- **Tests**: Unit tests with simulated CAN responses

### `obd/pid_decoder.h/cpp`
- **Responsibility**: Parse raw CAN frames into OBD values
- **Public API**:
  - `decode_response(frame, pid_id)` → OBD value
  - `supports_pid(pid_id)` → Bool
- **Tests**: Unit tests with known CAN/OBD pairs

### `transport/websocket_server.h/cpp`
- **Responsibility**: WebSocket protocol handling
- **Public API**:
  - `init(port)` → Start listening
  - `poll()` → Service connections
  - `broadcast(json_packet)` → Send to all clients
  - `num_clients()` → Connection count
- **Note**: May use IDF-native WebSocket library or minimal custom impl

### `transport/wifi_config.h/cpp`
- **Responsibility**: WiFi connection and discovery
- **Public API**:
  - `connect()` → Join SSID
  - `register_mdns(hostname)` → Advertise mDNS
  - `is_connected()` → State check
  - `get_ip_address()` → Return current IP

### `utils/json_builder.h/cpp`
- **Responsibility**: Build telemetry JSON packets
- **Public API**:
  - `build_telemetry(values, timestamp)` → JSON string
  - `build_status(conn_status)` → Status JSON
- **Note**: Minimal memory footprint, no external JSON lib

## Testing Architecture

### Unit Test Structure

```
test/
├── native_test/
│   ├── can_driver_test.cpp
│   ├── polling_engine_test.cpp
│   ├── pid_decoder_test.cpp
│   └── json_builder_test.cpp
└── integration/
    └── esp32_hardware_test.ino  (manual, upload to device)
```

**Native tests** run without hardware:
- Mock CAN frames as input
- Verify correct parsing and state transitions
- Fast feedback loop for TDD

**Hardware tests** run on ESP32:
- Real CAN bus communication
- Real WebSocket connections
- Verify end-to-end telemetry

## Key Design Decisions

### Why WebSocket over REST/HTTP?
- WebSocket is push-based → lower latency updates
- No polling overhead
- Works in web browsers natively
- Supports multiple concurrent clients

### Why hardcoded CAN config?
- Simplifies firmware (no config parsing)
- Prevents misconfiguration at the device level
- For custom config: rebuild firmware or use future SD card loader

### Why mDNS discovery?
- No need for users to find IP addresses manually
- Works on any WiFi network
- Simpler UX than manual IP entry

### Why GitHub Pages for dashboard?
- Zero hosting cost
- No server deployment needed
- Version control and CI/CD friendly
- Static hosting is fast and reliable
- Can work offline with proper PWA setup

## File Structure

```
c:\Users\chinn\workspace\esp-obd\
├── platformio.ini           # Build configuration
├── src/
│   ├── main.cpp             # Entry point
│   ├── can/
│   │   ├── can_driver.h/cpp
│   │   └── can_frame.h
│   ├── obd/
│   │   ├── polling_engine.h/cpp
│   │   ├── pid_decoder.h/cpp
│   │   └── pid_config.h
│   ├── transport/
│   │   ├── websocket_server.h/cpp
│   │   └── wifi_config.h/cpp
│   └── utils/
│       └── json_builder.h/cpp
├── include/
│   ├── can/
│   │   ├── can_config.h     # CAN constants (500kbps, 29-bit)
│   │   └── can_frame.h
│   ├── obd/
│   │   └── pid_config.h     # PID list
│   └── transport/
│       └── wifi_config.h    # WiFi/mDNS constants
├── test/
│   ├── native_toolchain.py
│   └── native/
│       ├── can_driver_test.cpp
│       ├── polling_engine_test.cpp
│       ├── pid_decoder_test.cpp
│       └── json_builder_test.cpp
├── dashboard/               # GitHub Pages
│   ├── index.html
│   ├── js/
│   │   ├── websocket-client.js
│   │   ├── display.js
│   │   └── config.js
│   ├── css/
│   │   └── app.css
│   └── manifest.webmanifest
├── PLAN.md                  # This plan
└── ARCHITECTURE.md          # This file
```

