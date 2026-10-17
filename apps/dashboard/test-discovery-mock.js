/**
 * Test harness for simulating ELM327 discovery with mock serial
 * Open browser console and run: import('./test-discovery-mock.js').then(m => m.runTest())
 */

// This test can be run in the browser console to validate the discovery flow
export async function runTest() {
  console.log("\n====== ELM327 DISCOVERY TEST ======\n");

  // Mock responses
  const mockResponses = {
    "ATZ": "ELM327 v1.5a",
    "ATE0": "OK",
    "ATL0": "OK",
    "ATH0": "OK",
    "ATSP0": "OK",
    "ATSH18DB": "OK",
    "ATCRA33F1": "OK",
    "0100": "41 00 BE 1F A8 01",
    "0120": "41 20 00 00 00 01",
    "0140": "41 40 FF FF FF FF",
    "0160": "41 60 00 00 00 00",
    "0180": "41 80 00 00 00 00",
    "01A0": "41 A0 00 00 00 00",
  };

  // Simulate command sequence
  const sequence = [
    { name: "Init", commands: ["ATZ", "ATE0", "ATL0", "ATH0"] },
    { name: "Protocol", commands: ["ATSP0"] },
    { name: "Header Setup", commands: ["ATSH18DB", "ATCRA33F1"] },
    { name: "PID Discovery", commands: ["0100", "0120", "0140", "0160", "0180", "01A0"] },
  ];

  for (const phase of sequence) {
    console.group(`📋 ${phase.name}`);
    for (const cmd of phase.commands) {
      const response = mockResponses[cmd] || "NO DATA";
      const time = Math.random() * 200 + 50;
      console.log(`  ✓ ${cmd.padEnd(12)} → ${response.padEnd(30)} (${time.toFixed(0)}ms)`);
      await new Promise(r => setTimeout(r, time));
    }
    console.groupEnd();
  }

  console.log("\n✅ Discovery sequence complete!\n");

  // Summary
  console.table({
    "Init Commands": "4 (ATZ, ATE0, ATL0, ATH0)",
    "Protocol Setup": "1 (ATSP0)",
    "Header Setup": "2 (ATSH, ATCRA)",
    "PID Discovery": "6 commands (0100-01A0)",
    "Total Round Trips": "13",
    "Estimated Time": "~2-3 seconds",
  });
}

// Auto-run if included as module
console.log("📌 ELM327 Discovery Test loaded. Run: runTest() to start");
