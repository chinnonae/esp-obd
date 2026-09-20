# Requirements & Implementation Mapping

## Your Requirements

### 1. Replace Bluetooth Serial with IP-based Communication

**Requirement**: The Bluetooth serial approach doesn't work reliably on mobile devices. Switch to WiFi + WebSocket.

**Implementation**:
- **Module**: `src/transport/websocket_server.h/cpp`
- **How it works**:
  - ESP32 connects to WiFi (SSID/password hardcoded in firmware)
  - ESP32 runs WebSocket server on port 8080
  - User manually enters ESP32 IP address in web dashboard
  - Web dashboard (on mobile) connects to `ws://192.168.1.100:8080` (or your IP)
  - Data flows: ESP32 → WebSocket → Browser → Gauges
  - **Finding the IP**: Use serial debug mode (see #2 below) to discover IP on startup
- **Advantages**:
  - More stable than Bluetooth Serial
  - Works with standard web browsers (no app store dependency)
  - Lower latency (IP + WebSocket)
  - Multiple clients can connect simultaneously
  - Self-hosted web app (no cloud dependency)
  - No complex mDNS discovery (mobile devices often disable it)
- **Testing**:
  - Unit test WebSocket frame encoding/decoding
  - Integration test: verify server accepts connection, broadcasts telemetry
  - System test: connect from real mobile phone via WiFi

---

### 2. Move Polling Logic to ESP32 (Server-side)

**Requirement**: The polling loop should run continuously on the ESP32. The app just connects and receives data. No more command-response exchanges between app and device.

**Implementation**:
- **Module**: `src/obd/polling_engine.h/cpp`
- **How it works**:
  ```
  ESP32 Main Loop (every ~10ms):
    ├─ Polling Engine tick()
    │  ├─ Decide which PID to request next
    │  ├─ Transmit CAN frame to vehicle
    │  ├─ Wait for ECU response
    │  └─ Parse response and update values
    └─ Every 100ms: serialize current values to JSON and broadcast via WebSocket

  Web App:
    └─ Just receives JSON packets and updates gauges
  ```
- **Why server-side polling is better**:
  - Consistent polling interval (no connection lag)
  - Vehicle gets predictable request pattern
  - App doesn't need to know about CAN/OBD details
  - Multiple clients can view same data without duplicate traffic
  - Easier to handle ECU timeouts and errors
- **Testing**:
  - Unit test polling state machine (tick through request/response cycle)
  - Unit test PID rotation and timeout handling
  - Integration test: verify continuous telemetry stream

---

### 3. Dashboard Hosted on GitHub Pages (Same as Before)

**Requirement**: Keep the dashboard as a static site on GitHub Pages (like the original).

**Implementation**:
- **Location**: `dashboard/` folder
- **How it works**:
  - Plain HTML/CSS/JavaScript (no build step needed)
  - Single HTML file (`index.html`) with embedded CSS/JS
  - User enters ESP32 IP or hostname on first load
  - Stored in browser localStorage for next visit
  - GitHub Actions deploys `dashboard/` to GitHub Pages
- **Files**:
  ```
  dashboard/
  ├── index.html                  # Main page + gauges
  ├── js/websocket-client.js      # Connect to ESP32, receive data
  ├── js/display.js               # Update gauges in real-time
  ├── js/config.js                # Store/retrieve ESP32 connection info
  ├── css/app.css                 # Styling for gauges, responsive
  └── manifest.webmanifest        # PWA metadata (installable on mobile)
  ```
- **User Experience**:
  1. Visit `https://yourusername.github.io/esp-obd/`
  2. Enter `esp-obd.local` or `192.168.1.100`
  3. Click "Connect"
  4. See live gauges
  5. Optional: "Install" as app on mobile home screen (PWA)
- **Advantages**:
  - Zero hosting cost
  - Automatic HTTPS (GitHub Pages)
  - Works offline (PWA cache)
  - Version control friendly
- **Testing**:
  - Manual: open dashboard in browser, verify gauges render
  - WebSocket connection: verify "Connected" indicator
  - Gauge update: verify values update in real-time

---

### 4. Hardcode CAN Settings (500kbps, 29-bit)

**Requirement**: No more ELM327 command layer. Remove all the `AT` commands and configuration variables. Fix CAN to 500kbps with 29-bit addressing.

**Implementation**:
- **Module**: `include/can/can_config.h`
  ```cpp
  namespace can_config {
    static constexpr uint32_t BITRATE = 500000;  // 500kbps, hardcoded
    static constexpr bool USE_EXTENDED_ID = true; // 29-bit IDs only
    static constexpr uint32_t TWAI_TX_GPIO = 26;  // ESP32 TWAI TX
    static constexpr uint32_t TWAI_RX_GPIO = 27;  // ESP32 TWAI RX
  }
  ```
- **What was removed**:
  - No more ELM327 command processing (no AT commands)
  - No more settings menus (no ELM-style variables)
  - No more protocol negotiation with device
  - No more multiple baudrate support
- **What stays**:
  - Standard SAE J1979 OBD-II PIDs (hardcoded list in `include/obd/pid_config.h`)
  - CAN frame parsing (ID, DLC, data)
  - Multiframe response handling (ISO-TP layer)
- **Why hardcoding is better**:
  - Firmware is smaller and simpler
  - No configuration errors possible
  - Faster startup (no negotiation)
  - Predictable behavior
  - If you need different settings: rebuild firmware (no user confusion)
- **Future flexibility**:
  - Can add SD card config loader later if needed
  - Can add serial debug commands (simple: `REBOOT`, `STATUS`, etc.)
  - Can add OTA firmware updates
- **Testing**:
  - Verify CAN initialized at startup: check TWAI registers
  - Verify bitrate is 500kbps: check TWAI_BRPR and TWAI_BTR
  - Verify 29-bit mode: check TWAI_MOD register

---

### 5. Serial Debug/Test Mode (Development Feature)

**Requirement**: Add an interactive serial interface via UART0 for testing and discovery during development.

**Implementation**:
- **Module**: `src/transport/serial_debug.h/cpp`
- **How it works**:
  - Connect USB-to-UART adapter to UART0 (GPIO1 TX, GPIO3 RX)
  - Open serial terminal: `pio device monitor -e ioxesp32`
  - Interactive commands available:
    ```
    > STATUS
    Device Status:
    - Uptime: 1234 seconds
    - WiFi IP: 192.168.1.100
    - WebSocket Port: 8080
    - Connected clients: 1
    - CAN frames received: 5678
    - Last poll cycle: 2500ms

    > TEST_PID 0x0C
    Sending PID 0x0C request...
    Response received: RPM = 1500

    > DISCOVER
    Scanning for supported PIDs...
    0x00: SUPPORTED
    0x0C: SUPPORTED (RPM)
    0x0D: SUPPORTED (Speed)
    0x0E: NOT_SUPPORTED
    ... (scanning 0x00-0xFF)
    Scan complete. Found 15 supported PIDs.

    > HELP
    Available commands:
    - STATUS             Show device status
    - TEST_PID <0xNN>    Test a specific PID
    - DISCOVER           Scan for supported PIDs
    - REBOOT             Restart device
    - HELP               Show this message
    ```
- **Why this is important for testability**:
  - **Find IP address**: Run `STATUS` to see WiFi IP before connecting web app
  - **Test individual PIDs**: `TEST_PID 0x0C` validates PID decoder without full polling
  - **Discover supported PIDs**: `DISCOVER` scans 0x00-0xFF to find what vehicle supports
  - **Debug issues**: `STATUS` shows real-time CAN frame count, connection state
  - **No mobile needed**: Can test ESP32 with just USB cable and terminal
- **Output format**:
  - All debug output prefixed with `#DBG: ` (distinguishable from user commands)
  - Example: `#DBG: Initialized CAN (500kbps, 29-bit)`
  - Command responses use plain text (easy to read)
- **Testing**:
  - Unit test command parsing
  - Integration test: verify commands execute and return correct responses
  - Manual test: connect via USB and issue commands

---

## Architecture Alignment

```
Your Requirements          → Architecture Module          → Testable?
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#1: IP + WebSocket         transport/websocket_server     ✓ Unit + Integration
#2: Server-side polling    obd/polling_engine             ✓ Unit tests
#3: GitHub Pages dashboard dashboard/ (HTML/CSS/JS)       ✓ Manual + integration
#4: Hardcoded CAN (500k)   can/can_driver + config        ✓ Unit + integration
#5: Serial debug/test      transport/serial_debug         ✓ Unit + integration
```

## Development Phases

### Phase 1: Core Logic (Testable without hardware)
- [ ] CAN frame structures and parsing
- [ ] OBD PID decoder (SAE J1979)
- [ ] Polling engine state machine
- [ ] JSON telemetry builder
- **Run**: `pio test -e native_test` (on your PC, instant feedback)

### Phase 2: Platform Integration (On ESP32)
- [ ] TWAI CAN driver initialization
- [ ] WiFi connection and mDNS
- [ ] WebSocket server
- [ ] Main loop connecting all pieces
- **Run**: Upload to ESP32, verify serial output

### Phase 3: Web Dashboard
- [ ] HTML layout with gauges
- [ ] WebSocket client (connect, receive, display)
- [ ] Connection UI and controls
- [ ] Deploy to GitHub Pages
- **Run**: Open in browser, connect to ESP32

### Phase 4: Validation
- [ ] Test with real vehicle or simulator
- [ ] Verify data accuracy
- [ ] Performance and stability testing
- [ ] Multiple client scenarios

## Success Criteria

✅ **Requirement 1**: Web app reliably connects from mobile phone over WiFi IP
✅ **Requirement 2**: ESP32 polls vehicle continuously without app intervention
✅ **Requirement 3**: Dashboard visible at GitHub Pages URL
✅ **Requirement 4**: CAN configuration is hardcoded, no ELM327 complexity

---

## Quick Reference: Where to Find Things

| Task | File | Notes |
|------|------|-------|
| Understand overall plan | `PLAN.md` | Read this first |
| System architecture & diagrams | `ARCHITECTURE.md` | Data flows, component interactions |
| How to write & run tests | `TESTING.md` | Unit test examples, integration steps |
| CAN frame structures | `src/can/can_frame.h` | 29-bit ID handling |
| OBD PID list | `include/obd/pid_config.h` | Hardcoded PIDs to poll |
| WebSocket protocol | `src/transport/websocket_server.h` | RFC 6455 implementation |
| Serial debug commands | `src/transport/serial_debug.h` | Interactive testing via UART0 |
| Main entry point | `src/main.cpp` | ESP32 setup() and loop() |
| Web dashboard | `dashboard/index.html` | HTML + embedded JS |
| Configuration constants | `include/*/config.h` | All hardcoded values (CAN, WiFi, etc.) |

