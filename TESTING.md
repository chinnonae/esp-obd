# Testing Strategy

## Philosophy

The codebase is organized to enable three levels of testing:
1. **Unit Tests** (fastest, no hardware needed)
2. **Integration Tests** (on real ESP32)
3. **System Tests** (with vehicle or simulator)

Each component has a public interface designed for testability.

## Unit Testing (Hardware-free)

Run with: `pio test -e native_test`

These tests compile and run on your development machine, fast feedback loop.

### CAN Driver Tests (`test/native/can_driver_test.cpp`)

Test the abstraction layer over TWAI hardware:

```cpp
TEST(CanDriver, SendsFrameWithCorrectId) {
  MockCanDriver driver;
  CanFrame frame{0x7DF, false, 8, {0x02, 0x10, 0x01, ...}};
  
  driver.send(frame);
  
  ASSERT_EQ(driver.last_sent_frame().can_id, 0x7DF);
}

TEST(CanDriver, ReceivesExtendedIds) {
  MockCanDriver driver;
  CanFrame response{0x7E8, true, 8, {0x03, 0x41, 0x00, 0x12, ...}};
  driver.inject_frame(response);  // Simulate received frame
  
  auto received = driver.receive();
  
  ASSERT_TRUE(received.has_value());
  ASSERT_EQ(received->can_id, 0x7E8);
}

TEST(CanDriver, Returns29BitIdCorrectly) {
  CanFrame frame{0x18DAF110, true, 8, {}};  // 29-bit ID
  ASSERT_TRUE(frame.is_extended);
  ASSERT_EQ(frame.can_id, 0x18DAF110);
}
```

**Key testable behaviors**:
- Frame encoding/decoding
- 29-bit ID handling
- Error conditions (timeouts, invalid frames)
- State initialization

### PID Decoder Tests (`test/native/pid_decoder_test.cpp`)

Test OBD value extraction from CAN frames:

```cpp
TEST(PidDecoder, DecodesRpm) {
  // CAN response: 03 41 0C 12 F8 (RPM = 0x12F8 / 4 = 1275)
  uint8_t data[] = {0x03, 0x41, 0x0C, 0x12, 0xF8, 0, 0, 0};
  CanFrame frame{0x7E8, true, 8, data};
  
  auto rpm = PidDecoder::decode(frame, 0x0C);
  
  ASSERT_TRUE(rpm.has_value());
  ASSERT_EQ(rpm->value, 1275);
  ASSERT_EQ(rpm->unit, "rpm");
}

TEST(PidDecoder, DecodesSpeed) {
  // CAN response: 03 41 0D 42 (Speed = 0x42 = 66 km/h)
  uint8_t data[] = {0x03, 0x41, 0x0D, 0x42, 0, 0, 0, 0};
  CanFrame frame{0x7E8, true, 8, data};
  
  auto speed = PidDecoder::decode(frame, 0x0D);
  
  ASSERT_EQ(speed->value, 66);
  ASSERT_EQ(speed->unit, "km/h");
}

TEST(PidDecoder, DecodesThrottlePosition) {
  // CAN response: 03 41 11 50 (Throttle = 0x50 * 100 / 255 = 31.4%)
  uint8_t data[] = {0x03, 0x41, 0x11, 0x50, 0, 0, 0, 0};
  CanFrame frame{0x7E8, true, 8, data};
  
  auto throttle = PidDecoder::decode(frame, 0x11);
  
  ASSERT_NEAR(throttle->value, 31.4, 0.1);
  ASSERT_EQ(throttle->unit, "%");
}

TEST(PidDecoder, SupportsOnlyImplementedPids) {
  ASSERT_TRUE(PidDecoder::supports_pid(0x0C));   // RPM
  ASSERT_TRUE(PidDecoder::supports_pid(0x0D));   // Speed
  ASSERT_FALSE(PidDecoder::supports_pid(0xFF));  // Not in our list
}
```

**Key testable behaviors**:
- Correct byte extraction and scaling
- Unit conversions
- PID support checking
- Error handling (malformed frames)

### Polling Engine Tests (`test/native/polling_engine_test.cpp`)

Test the state machine that orchestrates CAN communication:

```cpp
TEST(PollingEngine, InitializesWithPidList) {
  std::vector<uint8_t> pids = {0x0C, 0x0D, 0x11};
  PollingEngine engine;
  
  engine.init(pids);
  
  ASSERT_TRUE(engine.is_initialized());
  ASSERT_EQ(engine.get_next_pid(), 0x0C);
}

TEST(PollingEngine, RotatesThroughPids) {
  std::vector<uint8_t> pids = {0x0C, 0x0D, 0x11};
  PollingEngine engine;
  engine.init(pids);
  MockCanDriver can;
  
  // Cycle 1: Request PID 0x0C
  auto frame = engine.tick(&can, 0ms);
  ASSERT_EQ(frame.data[2], 0x0C);  // PID in request
  
  // Simulate response received
  uint8_t response[] = {0x03, 0x41, 0x0C, 0x12, 0xF8, 0, 0, 0};
  CanFrame rx{0x7E8, true, 8, response};
  can.inject_frame(rx);
  
  engine.tick(&can, 100ms);  // Process response
  ASSERT_EQ(engine.get_values()[0x0C].value, 1275);  // RPM decoded
  
  // Cycle 2: Should move to next PID
  frame = engine.tick(&can, 200ms);
  ASSERT_EQ(frame.data[2], 0x0D);  // Next PID
}

TEST(PollingEngine, HandlesTimeout) {
  PollingEngine engine;
  engine.init({0x0C});
  MockCanDriver can;
  
  auto frame = engine.tick(&can, 0ms);    // Send request
  engine.tick(&can, 1100ms);              // Timeout (>1000ms)
  
  // Should move to next PID and mark current as stale
  ASSERT_TRUE(engine.is_stale(0x0C));
  ASSERT_TRUE(engine.is_cycling());  // Still cycling, but moved on
}

TEST(PollingEngine, BuildsCorrectCanFrame) {
  PollingEngine engine;
  engine.init({0x0C});
  MockCanDriver can;
  
  auto frame = engine.get_current_request();
  
  // Should be standard OBD-II request
  ASSERT_EQ(frame.can_id, 0x7DF);        // OBD broadcast ID
  ASSERT_EQ(frame.dlc, 8);
  ASSERT_EQ(frame.data[0], 0x02);        // Number of data bytes
  ASSERT_EQ(frame.data[1], 0x10);        // Service: 0x01 (ReadData) + 0x0F (mode)
  ASSERT_EQ(frame.data[2], 0x0C);        // PID
}
```

**Key testable behaviors**:
- State transitions (idle → waiting → done)
- Correct PID rotation
- Timeout handling
- CAN frame construction
- Response parsing and value storage

### JSON Builder Tests (`test/native/json_builder_test.cpp`)

Test telemetry packet generation:

```cpp
TEST(JsonBuilder, BuildsTelemetryPacket) {
  std::map<uint8_t, ObdValue> values;
  values[0x0C] = {1275, "rpm"};
  values[0x0D] = {66, "km/h"};
  values[0x11] = {31.4, "%"};
  
  auto json = JsonBuilder::build_telemetry(values, 1695148532);
  
  ASSERT_THAT(json, HasSubstr("\"type\": \"telemetry\""));
  ASSERT_THAT(json, HasSubstr("\"rpm\": 1275"));
  ASSERT_THAT(json, HasSubstr("\"speed\": 66"));
  ASSERT_THAT(json, HasSubstr("\"throttle_position\": 31.4"));
  ASSERT_THAT(json, HasSubstr("\"timestamp\": 1695148532"));
}

TEST(JsonBuilder, ValidJson) {
  std::map<uint8_t, ObdValue> values;
  values[0x0C] = {1500, "rpm"};
  
  auto json = JsonBuilder::build_telemetry(values, 1234567890);
  
  // Verify it's valid JSON (can be parsed)
  ASSERT_TRUE(IsValidJson(json));
}

TEST(JsonBuilder, HandlesSpecialCharacters) {
  std::map<uint8_t, ObdValue> values;
  auto json = JsonBuilder::build_telemetry(values, 0);
  
  // Should not contain unescaped quotes or control chars
  ASSERT_THAT(json, Not(ContainsUnescapedQuotes()));
}
```

**Key testable behaviors**:
- Correct JSON structure
- Value serialization
- Valid JSON output
- Edge cases (zero values, null values, etc.)

## Integration Testing (On ESP32)

Run with: `pio run -e ioxesp32 -t upload` then manual verification

### Setup for Integration Tests

1. Connect ESP32 to development CAN bus or simulator
2. Upload firmware
3. Open serial monitor: `pio device monitor -e ioxesp32`
4. Connect web dashboard to `esp-obd.local:8080`
5. Verify telemetry displays

### Test Cases

**T01: Device Boots Successfully**
```
Steps:
1. Power cycle ESP32
2. Watch serial output

Expected:
- #DBG: Initialized CAN (500kbps, 29-bit)
- #DBG: Connecting to WiFi...
- #DBG: Connected. IP: 192.168.1.100
- #DBG: mDNS registered as esp-obd.local
- #DBG: WebSocket server listening on :8080
- #DBG: Starting polling engine
```

**T02: CAN Frames Transmitted**
```
Steps:
1. Boot ESP32
2. Attach CAN analyzer to bus
3. Wait 1-2 polling cycles

Expected:
- See CAN frames with ID 0x7DF
- Frames contain mode 0x01 (ReadData) requests
- Frames alternate between different PIDs (0x0C, 0x0D, 0x11, etc.)
```

**T03: WebSocket Connection Accepted**
```
Steps:
1. Boot ESP32
2. From web dashboard, enter IP/hostname and click Connect

Expected:
- Connection succeeds
- Serial output: #DBG: WebSocket client connected
- Web dashboard enters "connected" state (green indicator)
```

**T04: Telemetry Received and Displayed**
```
Steps:
1. Connect web dashboard
2. Wait for 2-3 polling cycles
3. Watch gauge display

Expected:
- Values appear on gauges (may show 0 if not connected to vehicle)
- New values received every ~1 second
- Timestamp updates
- Connection indicator stays green
```

**T05: Graceful Disconnect**
```
Steps:
1. Connected to dashboard
2. Close browser tab or disconnect network

Expected:
- Serial output: #DBG: WebSocket client disconnected
- Server continues polling
- Server can accept new connections
```

## System Testing (With Vehicle/Simulator)

### CAN Bus Simulator Setup

For testing without a vehicle:

```bash
# Use virtual CAN bus on Linux
candump vcan0
cansend vcan0 7E8#03410C12F8  # Simulate ECU response
```

Or use commercial OBD simulators that transmit real ECU responses over CAN.

### Validation Checklist

- [ ] Verify RPM matches vehicle gauge
- [ ] Verify speed matches speedometer
- [ ] Verify coolant temp matches dashboard
- [ ] Verify all hardcoded PIDs are working
- [ ] Verify dashboard responsive on 4G/WiFi
- [ ] Verify multiple phones can connect simultaneously
- [ ] Verify no data corruption over 24+ hour run
- [ ] Verify clean disconnect and reconnect

## Continuous Integration (Future)

Once tests are established, can add CI:

```yaml
# .github/workflows/test.yml
- name: Run Unit Tests
  run: pio test -e native_test
  
- name: Build Firmware
  run: pio build -e ioxesp32
```

## Debugging Tools

### Serial Output Format

All debug output prefixed with `#DBG:` for easy filtering:

```
#DBG: CAN initialized (500kbps, 29-bit)
#DBG: Sending PID 0x0C request to 0x7DF
#DBG: Received frame 0x7E8 with 8 bytes
#DBG: Decoded RPM: 1275
#DBG: WebSocket broadcast to 2 clients
```

### Mock Objects

Provide mock implementations for:
- `MockCanDriver` - Inject fake CAN frames
- `MockWiFiInterface` - Fake network state
- `MockWebSocketServer` - Verify broadcast calls

These allow unit tests to run on PC without hardware.

### Logging Levels (Future Enhancement)

```cpp
enum LogLevel { DEBUG, INFO, WARN, ERROR };
void set_log_level(LogLevel level);  // Control verbosity
```

Can be adjusted via compile flag or (future) runtime config.

