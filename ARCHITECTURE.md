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
│  │  │  Serial Debug Interface (UART0)                 │ │ │
│  │  │  - Interactive commands: STATUS, TEST_PID, etc. │ │ │
│  │  │  - Connect via USB-to-UART adapter              │ │ │
│  │  │  - Used for testing and PID discovery           │ │ │
│  │  │  - Run: pio device monitor -e ioxesp32         │ │ │
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
│  │  │  - Maintain WebSocket connections             │   │ │
│  │  │  - No mDNS (mobile devices often disable it)   │   │ │
│  │  │  - User finds IP via serial debug interface    │   │ │
│  │  └────────────────────────────────────────────────┘   │ │
│  └───────────────────────────────────────────────────────┘ │
│                                                             │
│  ┌───────────────────────────────────────────────────────┐ │
│  │              Hardware Interfaces                      │ │
│  │                                                       │ │
│  │  CAN Bus ─────► ESP32 TWAI (GPIO 26/27, 500kbps)   │ │
│  │  Serial (UART0) ─► Debug interface (GPIO1 TX/3 RX)  │ │
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
   └─► Print debug messages: #DBG: Initialized...
   └─► Initialize CAN (TWAI, 500kbps, 29-bit)
   └─► Connect WiFi (SSID/password hardcoded)
   └─► Print IP address: #DBG: WiFi connected. IP: 192.168.1.100
   └─► Start WebSocket Server (port 8080)
   └─► Begin Polling Engine
   └─► Print prompt: > (ready for serial commands)

2. Developer Connects to Serial
   └─► Run: pio device monitor -e ioxesp32
   └─► Type: STATUS
   └─► See: Device IP: 192.168.1.100, WebSocket Port: 8080

3. Web Dashboard Opens (on mobile)
   └─► User enters IP address: 192.168.1.100 (found from serial STATUS)
   └─► Click "Connect"

4. WebSocket Client Connects
   └─► WebSocket client connects to ws://192.168.1.100:8080
   └─► Server sends initial state
   └─► Web app enters "connected" state
   └─► Serial shows: #DBG: WebSocket client connected
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
- **Responsibility**: WiFi connection and network management
- **Public API**:
  - `connect()` → Join SSID
  - `is_connected()` → State check
  - `get_ip_address()` → Return current IP (printed to serial)
- **Note**: No mDNS (mobile devices often disable it). User finds IP via serial debug interface.

### `transport/serial_debug.h/cpp`
- **Responsibility**: Interactive debug interface via UART0
- **Public API**:
  - `init()` → Initialize serial 115200 baud
  - `poll()` → Check for input, execute commands
  - `print(msg)` → Output (prefixed with `#DBG: ` for auto messages)
- **Commands**:
  - `STATUS` → Print device status (IP, connections, CAN stats)
  - `TEST_PID <0xNN>` → Test a single PID, show response
  - `DISCOVER` → Scan 0x00-0xFF to find supported PIDs
  - `REBOOT` → Restart device
  - `HELP` → Show available commands
- **Output format**: 
  - Auto messages: `#DBG: <message>` (printed without prompt)
  - Command responses: Plain text (printed after `> `)
  - Prompt: `> ` (shown when ready for input)
- **Tests**: Unit tests with simulated serial input/output

### `utils/json_builder.h/cpp`
- **Responsibility**: Build telemetry JSON packets
- **Public API**:
  - `build_telemetry(values, timestamp)` → JSON string
  - `build_status(conn_status)` → Status JSON
- **Note**: Minimal memory footprint, no external JSON lib

## Serial Debug Interface

The ESP32 exposes an interactive debug interface via UART0 for development and testing.

### Usage

```bash
# Open serial monitor
pio device monitor -e ioxesp32

# You should see:
# #DBG: Initialized CAN (500kbps, 29-bit)
# #DBG: Connecting to WiFi...
# #DBG: Connected. IP: 192.168.1.100
# #DBG: WebSocket server listening on :8080
# #DBG: Starting polling engine
# >

# Now you can type commands:
> STATUS
Device Status:
- Uptime: 1234 seconds
- WiFi IP: 192.168.1.100
- WebSocket Port: 8080
- Connected clients: 0
- CAN frames received: 5678
- Last CAN error: None

> TEST_PID 0x0C
#DBG: Sending PID 0x0C request...
#DBG: Received response
RPM = 1500

> TEST_PID 0x0D
#DBG: Sending PID 0x0D request...
#DBG: Received response
Speed = 65 km/h

> DISCOVER
#DBG: Scanning for supported PIDs (0x00-0xFF)...
0x00: SUPPORTED
0x0C: SUPPORTED (RPM)
0x0D: SUPPORTED (Speed)
0x0E: NOT_SUPPORTED
0x10: SUPPORTED (MAF Air Flow)
... [more PIDs]
#DBG: Scan complete. Found 15 supported PIDs.

> REBOOT
#DBG: Rebooting...
[device restarts]
```

### Output Format

**Auto Messages** (printed without prompt):
- Format: `#DBG: <message>`
- Example: `#DBG: WebSocket client connected`
- When: Automatically by firmware (connection, errors, events)

**Command Responses** (printed after `> ` prompt):
- Format: Plain text, human-readable
- Example: `Device Status:` followed by details
- When: In response to user commands

**Command Prompt**:
- Format: `> `
- When: Ready for next command

### Purpose

The serial debug interface serves several purposes during development and testing:

1. **Finding the IP Address**: Run `STATUS` to see the WiFi IP (needed to connect web app)
2. **Testing Individual PIDs**: `TEST_PID 0x0C` validates PID decoder without full polling loop
3. **Discovering Supported PIDs**: `DISCOVER` scans the ECU to find what data it provides
4. **Debugging Issues**: `STATUS` shows CAN frame count, connection state, errors
5. **Device Control**: `REBOOT` restarts if needed

### Hardware Setup

- **Serial Port**: UART0 (GPIO 1 TX, GPIO 3 RX)
- **Baud Rate**: 115200
- **No Flow Control**: CTS/RTS not used
- **Connection**: USB-to-UART adapter (e.g., CP2102, CH340)

### Implementation Notes

- Serial processing runs in main `loop()`, non-blocking
- Commands parsed as simple text (space-delimited)
- Responses printed directly (no JSON)
- No authentication or command rate limiting (development tool only)

## Testing Architecture

### Unit Test Structure

```
test/
├── native_test/
│   ├── can_driver_test.cpp
│   ├── polling_engine_test.cpp
│   ├── pid_decoder_test.cpp
│   ├── json_builder_test.cpp
│   └── serial_debug_test.cpp
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

