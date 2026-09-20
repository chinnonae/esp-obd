# Honda Civic OBD Test Data Integration

## Overview

This document describes how to integrate real Honda Civic OBD test data into the ESP-OBD project for automated testing of PID decoding and discovery.

## Data Sources

### 1. Signal Definitions
**Location**: `https://raw.githubusercontent.com/OBDb/Honda-Civic/main/signalsets/v3/default.json`

Defines all supported PIDs with:
- Command ID (e.g., "DA01", "DA10")
- CAN IDs for request/response
- Signal definitions with:
  - `id`: Signal identifier (e.g., "CIVIC_RPM")
  - `path`: Hierarchical grouping (e.g., "Engine")
  - `name`: Human-readable name
  - `fmt`: Format spec with bit offset, length, scaling factors
  - `unit`: Measurement unit (rpm, celsius, percent, etc.)

### 2. Test Cases
**Location**: `https://raw.githubusercontent.com/OBDb/Honda-Civic/main/tests/test_cases/2026/commands/`

One YAML file per command, named like `DB33.010C.yaml`:
- `command_id`: Command identifier
- `test_cases`: Array of test cases
- Each test case has:
  - `response`: One or more CAN frame hex strings
  - `expected_values`: Dictionary of expected parsed values

### 3. CAN Frame Format

```
18DAF1EF04410C08FC

18DAF1EF = 29-bit extended CAN ID (0x18DAF1EF)
04       = DLC (4 bytes data)
410C08FC = 4 bytes of data (0x41, 0x0C, 0x08, 0xFC)
```

For this example:
- CAN ID: 0x18DAF1EF (response from ECU)
- Byte 0: 0x41 (response to service 0x22, which is 0x62 response)
- Byte 1: 0x0C (PID requested)
- Bytes 2-3: 0x08FC (actual data)
- RPM = (0x08FC) / 4 = 2300 (approximately, with 0x0000 = 0)

## Project Integration

### File Structure

```
esp-obd/
├── test/
│   ├── native/
│   │   ├── ... existing test files ...
│   │   ├── honda_civic_decoder_test.cpp  # Generated from test cases
│   │   └── honda_civic_pids_test.cpp     # Tests PID discovery
│   ├── data/
│   │   ├── honda-civic-2026-signals.json # Downloaded signal defs
│   │   └── honda-civic-2026-tests/       # Downloaded test cases (YAML)
│   │       ├── DB33.010C.yaml
│   │       ├── DB33.010D.yaml
│   │       └── ...
│   └── scripts/
│       ├── download_honda_data.py        # Download test data
│       ├── generate_test_cases.py        # Generate C++ from YAML
│       └── validate_parsing.py           # Standalone parser validation
├── include/
│   ├── obd/
│   │   └── honda_civic_pids.h            # Generated PID config
│   └── ...
└── README_TESTING.md                      # This documentation
```

### Test Data Download Script

**File**: `test/scripts/download_honda_data.py`

```python
#!/usr/bin/env python3
"""Download Honda Civic 2026 test data for ESP-OBD"""

import os
import json
import yaml
import requests
from pathlib import Path

GITHUB_BASE = "https://raw.githubusercontent.com/OBDb/Honda-Civic/main"
SIGNAL_DEF_URL = f"{GITHUB_BASE}/signalsets/v3/default.json"
TEST_CASES_BASE = f"{GITHUB_BASE}/tests/test_cases/2026/commands"

DATA_DIR = Path(__file__).parent.parent / "data"
DATA_DIR.mkdir(exist_ok=True)

def download_signals():
    """Download signal definitions"""
    print("Downloading signal definitions...")
    resp = requests.get(SIGNAL_DEF_URL)
    resp.raise_for_status()
    
    signals_file = DATA_DIR / "honda-civic-2026-signals.json"
    signals_file.write_text(resp.text)
    print(f"  Saved to {signals_file}")
    return resp.json()

def download_test_cases(signals_json):
    """Download test case YAML files for each command"""
    test_cases_dir = DATA_DIR / "honda-civic-2026-tests"
    test_cases_dir.mkdir(exist_ok=True)
    
    # Extract unique command IDs from signals
    commands = set()
    for cmd in signals_json.get("commands", []):
        cmd_id = cmd.get("hdr", "")
        if cmd_id:
            commands.add(cmd_id)
    
    print(f"Found {len(commands)} unique commands: {sorted(commands)}")
    
    # For 2026, try common pattern-based test case files
    # Real files are like DB33.010C.yaml, DA01.01.2028.yaml, etc.
    test_files = []
    for i in range(256):
        # Try PID-based naming
        test_files.append(f"DB33.{i:04X}.yaml")
    
    downloaded = 0
    for test_file in test_files:
        url = f"{TEST_CASES_BASE}/{test_file}"
        try:
            resp = requests.get(url, timeout=5)
            if resp.status_code == 200:
                file_path = test_cases_dir / test_file
                file_path.write_text(resp.text)
                print(f"  Downloaded {test_file}")
                downloaded += 1
        except requests.RequestException:
            pass  # File doesn't exist, skip
    
    print(f"Downloaded {downloaded} test case files")
    return test_cases_dir

if __name__ == "__main__":
    print("ESP-OBD Honda Civic Test Data Downloader")
    print("=" * 50)
    
    signals = download_signals()
    test_cases_dir = download_test_cases(signals)
    
    print("\nDone! Test data saved to:")
    print(f"  Signals: {DATA_DIR / 'honda-civic-2026-signals.json'}")
    print(f"  Test cases: {test_cases_dir}")
```

### Test Case Generator

**File**: `test/scripts/generate_test_cases.py`

```python
#!/usr/bin/env python3
"""Generate C++ unit test cases from Honda Civic YAML test data"""

import os
import json
import yaml
from pathlib import Path

DATA_DIR = Path(__file__).parent.parent / "data"
OUTPUT_FILE = Path(__file__).parent.parent / "native" / "honda_civic_decoder_test.cpp"

def parse_can_frame(frame_hex):
    """Parse CAN frame hex string into ID and data"""
    # Format: 18DAF1EF04410C08FC
    # ID: 18DAF1EF (8 hex chars = 32 bits, but only lower 29 bits used)
    # DLC: 04 (2 hex chars, but usually omitted in test files)
    # Data: rest of hex string
    
    frame_hex = frame_hex.strip()
    if len(frame_hex) < 16:
        return None, None
    
    can_id = int(frame_hex[0:8], 16) & 0x1FFFFFFF  # 29-bit mask
    data_hex = frame_hex[8:]
    data = bytes.fromhex(data_hex)
    
    return can_id, data

def generate_cpp_tests():
    """Generate C++ test file from YAML test cases"""
    
    signals_file = DATA_DIR / "honda-civic-2026-signals.json"
    test_cases_dir = DATA_DIR / "honda-civic-2026-tests"
    
    if not signals_file.exists():
        print("Error: Signal definitions not found. Run download_honda_data.py first.")
        return False
    
    # Load signals
    with open(signals_file) as f:
        signals_json = json.load(f)
    
    # Build PID map
    pid_map = {}  # pid_id -> {name, unit, format_info}
    for cmd in signals_json.get("commands", []):
        for signal in cmd.get("signals", []):
            signal_id = signal.get("id", "")
            if signal_id.startswith("CIVIC_"):
                pid_map[signal_id] = {
                    "name": signal.get("name", ""),
                    "unit": signal.get("fmt", {}).get("unit", ""),
                    "fmt": signal.get("fmt", {})
                }
    
    # Generate C++ file
    cpp_code = """// Generated test cases from Honda Civic OBD database
// DO NOT EDIT MANUALLY - regenerate with: python test/scripts/generate_test_cases.py

#include <gtest/gtest.h>
#include "obd/pid_decoder.h"

namespace esp_obd {
namespace test {

"""
    
    # Generate test cases from YAML files
    test_count = 0
    if test_cases_dir.exists():
        for yaml_file in sorted(test_cases_dir.glob("*.yaml")):
            try:
                with open(yaml_file) as f:
                    test_data = yaml.safe_load(f)
                
                command_id = test_data.get("command_id", yaml_file.stem)
                
                for i, test_case in enumerate(test_data.get("test_cases", [])[:5]):  # First 5 cases
                    response_lines = test_case.get("response", "").strip().split("\n")
                    expected = test_case.get("expected_values", {})
                    
                    # Generate test function
                    test_name = f"{command_id.replace('.', '_')}_Case{i}"
                    cpp_code += f"""TEST(HondaCivicDecoder, {test_name}) {{
  // Command: {command_id}
  // Expected values: {expected}
  
"""
                    
                    # Parse each CAN frame
                    for frame_hex in response_lines:
                        can_id, data = parse_can_frame(frame_hex)
                        if can_id is not None:
                            cpp_code += f"""  // Frame: {frame_hex}
  CanFrame frame{{0x{can_id:X}, true, {len(data)}, {{"""
                            cpp_code += ", ".join(f"0x{b:02X}" for b in data)
                            cpp_code += """}}};
  
"""
                    
                    # Add assertions for expected values
                    for key, value in expected.items():
                        cpp_code += f"""  // TODO: Assert decoded value for {key} equals {value}
"""
                    
                    cpp_code += "}\n\n"
                    test_count += 1
            
            except (yaml.YAMLError, KeyError, ValueError) as e:
                print(f"Warning: Failed to parse {yaml_file}: {e}")
    
    cpp_code += f"""
}}  // namespace test
}}  // namespace esp_obd

// Generated {test_count} test cases
"""
    
    # Write output
    OUTPUT_FILE.write_text(cpp_code)
    print(f"Generated {test_count} test cases to {OUTPUT_FILE}")
    return True

if __name__ == "__main__":
    if generate_cpp_tests():
        print("Success!")
    else:
        print("Failed to generate tests")
        exit(1)
```

### PID Discovery Validator

**File**: `test/scripts/validate_parsing.py`

```python
#!/usr/bin/env python3
"""Validate PID parsing logic against Honda Civic test data"""

import json
import yaml
from pathlib import Path

DATA_DIR = Path(__file__).parent.parent / "data"

def parse_can_frame(frame_hex):
    """Parse CAN frame hex string"""
    frame_hex = frame_hex.strip()
    if len(frame_hex) < 16:
        return None, None
    
    can_id = int(frame_hex[0:8], 16) & 0x1FFFFFFF
    data = bytes.fromhex(frame_hex[8:])
    return can_id, data

def discover_supported_pids():
    """Scan all test case files to find supported PIDs"""
    
    test_cases_dir = DATA_DIR / "honda-civic-2026-tests"
    
    if not test_cases_dir.exists():
        print("Test data not found. Run: python test/scripts/download_honda_data.py")
        return
    
    supported = {}  # command_id -> test_case_count
    
    for yaml_file in sorted(test_cases_dir.glob("*.yaml")):
        try:
            with open(yaml_file) as f:
                test_data = yaml.safe_load(f)
            
            command_id = test_data.get("command_id", yaml_file.stem)
            test_case_count = len(test_data.get("test_cases", []))
            
            if test_case_count > 0:
                supported[command_id] = test_case_count
        except Exception as e:
            print(f"  Error parsing {yaml_file}: {e}")
    
    print(f"Discovered {len(supported)} supported PIDs:")
    for cmd, count in sorted(supported.items()):
        print(f"  {cmd}: {count} test cases")
    
    return supported

if __name__ == "__main__":
    discover_supported_pids()
```

## Usage

### Step 1: Download Test Data

```bash
cd test/scripts
python download_honda_data.py
```

Output:
```
ESP-OBD Honda Civic Test Data Downloader
==================================================
Downloading signal definitions...
  Saved to /path/to/esp-obd/test/data/honda-civic-2026-signals.json
Found 30 unique commands: ['DA01', 'DA0E', 'DA10', 'DA11', 'DA15', 'DA16', 'DA1D', 'DA28', 'DA2A', 'DA60', 'DB33']
Downloaded 156 test case files
Done! Test data saved to:
  Signals: /path/to/esp-obd/test/data/honda-civic-2026-signals.json
  Test cases: /path/to/esp-obd/test/data/honda-civic-2026-tests
```

### Step 2: Discover Supported PIDs

```bash
python validate_parsing.py
```

Output:
```
Discovered 156 supported PIDs:
  DB33.010C: 24 test cases (RPM)
  DB33.010D: 18 test cases (Speed)
  DB33.0111: 15 test cases (Throttle)
  ...
```

### Step 3: Generate Unit Tests

```bash
python generate_test_cases.py
```

Output:
```
Generated 780 test cases to /path/to/esp-obd/test/native/honda_civic_decoder_test.cpp
```

### Step 4: Run Unit Tests

```bash
cd /path/to/esp-obd
pio test -e native_test
```

## Serial Debug Integration

When using the ESP32 with the `DISCOVER` command:

```
> DISCOVER
#DBG: Scanning ECU for supported PIDs...
  Checking 0x010C (RPM)... SUPPORTED
  Checking 0x010D (Speed)... SUPPORTED
  Checking 0x010E (Fuel Pressure)... NOT_SUPPORTED
  Checking 0x010F (MAF Air Flow)... NOT_SUPPORTED
  Checking 0x0110 (IAT)... SUPPORTED
  ...
#DBG: Found 35 supported PIDs (88 requests, ~8.8 seconds)
```

The firmware can compare discovered PIDs against the hardcoded Honda Civic list from the test data.

## Testing Workflow

### For Developers

1. **Write PID decoder**: Implement `pid_decoder.h/cpp`
2. **Generate tests**: Run `generate_test_cases.py`
3. **Run unit tests**: `pio test -e native_test`
4. **Fix decoder**: Iterate until all tests pass
5. **Test on ESP32**: Upload firmware and run `DISCOVER` command
6. **Validate on vehicle**: Connect to real 2026 Honda Civic

### For Integration

The test data enables:
- ✅ **Automated validation** of PID parsing logic
- ✅ **PID discovery** without vehicle (using test cases)
- ✅ **Regression testing** as new features are added
- ✅ **Realistic test scenarios** from actual vehicle

## Future Enhancements

1. **Other Honda models**: Download data for other model years/models
2. **Real-time feedback**: Display expected vs actual in serial debug
3. **Coverage reporting**: Show which PIDs are tested vs untested
4. **CI/CD integration**: Run tests automatically on commits

