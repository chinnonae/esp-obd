/**
 * ELM327 Session Mock - Simulates hardware responses for testing
 * Mimics the init sequence and discovery commands
 */

// Simulated responses for ELM327 commands
const RESPONSES = {
  // Init sequence
  "ATZ": "ELM327 v1.5a\r\n>",
  "ATE0": "OK\r\n>",
  "ATL0": "OK\r\n>",
  "ATH0": "OK\r\n>",

  // Header/address setting
  "ATSH18DB": "OK\r\n>",
  "ATCRA33F1": "OK\r\n>",
  "ATSH18DA01": "OK\r\n>",
  "ATCRAF1": "OK\r\n>",

  // Protocol setting (auto-search)
  "ATSP0": "OK\r\n>",

  // PID Discovery - Mode 01 bitmask queries
  "0100": "41 00 BE 1F A8 01\r\n>",     // Supported PIDs 01-20
  "0120": "41 20 00 00 00 01\r\n>",     // Supported PIDs 21-40
  "0140": "41 40 FF FF FF FF\r\n>",     // Supported PIDs 41-60 (all supported)
  "0160": "41 60 00 00 00 00\r\n>",     // Supported PIDs 61-80 (none)
  "0180": "41 80 00 00 00 00\r\n>",     // Supported PIDs 81-A0 (none)
  "01A0": "41 A0 00 00 00 00\r\n>",     // Supported PIDs A1-C0 (none)

  // Custom Mode 22 commands
  "DA012028": "62 28 04 D5 04 C8 04 CA 04 CB 04 CA 04 C7 04 C3\r\n>",   // Sample battery cells
  "DA01202A": "62 2A 64 01 C8 D0 01 08 00\r\n>",                        // SOC + voltage

  // Fallback for unknown commands
  "*": "?\r\n>"
};

/**
 * Simulates sending a command to ELM327 and getting a response
 * Logs the sequence for verification
 */
export function simulateCommand(command) {
  const response = RESPONSES[command] || RESPONSES["*"];
  console.log(`→ TX: ${command}`);
  console.log(`← RX: ${response.trim()}`);
  return response;
}

/**
 * Simulates the full init + discovery sequence
 */
export async function simulateFullSession() {
  console.log("\n========== ELM327 MOCK SESSION ==========\n");

  const commands = [
    // Init sequence
    "ATZ",
    "ATE0",
    "ATL0",
    "ATH0",

    // Protocol selection
    "ATSP0",

    // Header setup for Mode 01 discovery
    "ATSH18DB",
    "ATCRA33F1",

    // PID discovery sequence
    "0100",
    "0120",
    "0140",
    "0160",
    "0180",
    "01A0",

    // Header setup for Mode 22 commands
    "ATSHDA01",
    "ATCRAF1",

    // Custom command discovery
    "DA012028",
    "DA01202A",
  ];

  for (const cmd of commands) {
    simulateCommand(cmd);
    // Simulate network delay
    await new Promise(resolve => setTimeout(resolve, 100));
  }

  console.log("\n========== SESSION COMPLETE ==========\n");
}

// Run if executed directly
if (import.meta.url === `file://${process.argv[1]}`) {
  simulateFullSession();
}
