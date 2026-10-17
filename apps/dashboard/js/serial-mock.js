/**
 * Mock Serial Adapter - Replaces Web Serial for testing
 * Simulates ELM327 device responses to debug discovery/polling
 */

const RESPONSES = {
  // Init sequence
  "ATZ": "ELM327 v1.5a",
  "ATE0": "OK",
  "ATL0": "OK",
  "ATH0": "OK",

  // Header/address setting
  "ATSH18DB": "OK",
  "ATCRA33F1": "OK",
  "ATSHDA01": "OK",
  "ATCRAF1": "OK",
  "ATSHDAFF": "OK",
  "ATCRA33": "OK",

  // Protocol
  "ATSP0": "OK",

  // Mode 01 PID Discovery
  "0100": "41 00 BE 1F A8 01",     // Supported PIDs 01-20
  "0120": "41 20 00 00 00 01",     // Supported PIDs 21-40
  "0140": "41 40 FF FF FF FF",     // Supported PIDs 41-60 (all)
  "0160": "41 60 00 00 00 00",     // Supported PIDs 61-80
  "0180": "41 80 00 00 00 00",     // Supported PIDs 81-A0
  "01A0": "41 A0 00 00 00 00",     // Supported PIDs A1-C0

  // Mode 22/23 commands (custom)
  "DA012028": "62 28 04 D5 04 C8 04 CA 04 CB 04 CA 04 C7 04 C3",
  "DA01202A": "62 2A 64 01 C8 D0 01 08 00",
  "DAFF2028": "NO DATA",           // Some commands return NO DATA
};

let isConnected = false;
let portIsOpen = false;
const listeners = new Set();

function emit(event) {
  for (const listener of listeners) {
    listener(event);
  }
}

export async function connect() {
  console.log("[Mock] Connecting...");
  isConnected = true;
  portIsOpen = true;
  emit({ direction: "system", text: "[Mock] Connected", timestamp: Date.now() });
}

export function isConnectedState() {
  return isConnected;
}

export async function disconnect() {
  console.log("[Mock] Disconnecting...");
  isConnected = false;
  portIsOpen = false;
}

export function onEvent(callback) {
  listeners.add(callback);
  return () => listeners.delete(callback);
}

export async function sendCommand(text) {
  if (!isConnected) {
    throw new Error("Not connected");
  }

  // Simulate sending
  emit({ direction: "tx", text, timestamp: Date.now() });
  console.log(`[Mock TX] ${text}`);

  // Simulate delay (network round-trip)
  await new Promise(resolve => setTimeout(resolve, 50 + Math.random() * 100));

  // Get response
  const response = RESPONSES[text] || "NO DATA";
  console.log(`[Mock RX] ${response}`);

  // Emit response
  emit({ direction: "rx", text: response, timestamp: Date.now() });

  return response;
}

// Export utility for running full session
export async function runMockSession() {
  console.log("\n========== MOCK ELM327 SESSION ==========\n");

  const commands = [
    // Init
    "ATZ", "ATE0", "ATL0", "ATH0", "ATSP0",

    // Mode 01 discovery setup
    "ATSH18DB", "ATCRA33F1",

    // PID discovery
    "0100", "0120", "0140", "0160", "0180", "01A0",

    // Mode 22 setup
    "ATSHDA01", "ATCRAF1",

    // Custom commands
    "DA012028", "DA01202A",
  ];

  for (const cmd of commands) {
    try {
      const response = await sendCommand(cmd);
      console.log(`✓ ${cmd} → ${response.split("\r")[0]}`);
    } catch (e) {
      console.error(`✗ ${cmd} → ERROR: ${e.message}`);
    }
    await new Promise(resolve => setTimeout(resolve, 50));
  }

  console.log("\n========== SESSION COMPLETE ==========\n");
}
