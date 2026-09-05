// Discovers supported Mode 01 PIDs from OBD-II bitmask responses.
// Queries 0100, 0120, 0140, 0160, 0180, 01A0 to determine which PIDs
// the vehicle supports, then returns a Set of supported PID hex strings.

const ERROR_RESPONSE = /^(NO DATA|UNABLE TO CONNECT|ERROR|BUS INIT|STOPPED|\?)/i;

/**
 * Parses raw response text into bytes (first line only, multi-ECU support).
 */
function parseResponseBytes(text) {
  const firstLine = text.split("\r")[0].trim();
  if (!firstLine || ERROR_RESPONSE.test(firstLine)) {
    return null;
  }
  const tokens = firstLine.split(/\s+/).filter(Boolean);
  if (tokens.length === 0 || tokens.some((tok) => !/^[0-9A-Fa-f]{2}$/.test(tok))) {
    return null;
  }
  return tokens.map((tok) => parseInt(tok, 16));
}

/**
 * Strips the mode+PID echo bytes from a response.
 * Mode 01 responses start with 0x41 (0x01 + 0x40), followed by the PID.
 */
function stripEcho(bytes) {
  // For Mode 01 (0x41 response), skip 2 bytes: mode (0x41) + PID (1 byte)
  if (bytes.length < 2 || bytes[0] !== 0x41) {
    return null;
  }
  return bytes.slice(2); // Skip 0x41 and PID byte
}

/**
 * Parses a 4-byte bitmask (32 bits) and returns supported PID numbers.
 * Each bit represents a PID. Bit 0 = PID N, Bit 1 = PID N+1, etc.
 * (Note: This is actually MSB-first bit ordering in the standard.)
 */
function parsePidBitmask(bytes, pidBase) {
  const supportedPids = [];
  for (let i = 0; i < bytes.length && i < 4; i++) {
    const byte = bytes[i];
    for (let bit = 0; bit < 8; bit++) {
      if ((byte >> (7 - bit)) & 1) {
        const pid = pidBase + i * 8 + bit;
        supportedPids.push(pid);
      }
    }
  }
  return supportedPids;
}

/**
 * Converts a PID number to hex string with leading zero if needed.
 */
function pidToHex(pid) {
  return pid.toString(16).toUpperCase().padStart(2, "0");
}

/**
 * Discovers supported Mode 01 PIDs by querying discovery commands.
 * Returns a Map of { pidHex: pidNumber } for all supported PIDs.
 */
export async function discoverSupportedPids(serial, elm) {
  const supportedPids = new Map(); // pidHex -> pidNumber
  
  // Discovery commands and their corresponding PID ranges
  // Format: [discoveryCmd, pidBase]
  // Each command returns a 32-bit bitmask for PIDs in its range
  // 0100: bitmask for PIDs 0x01-0x20
  // 0120: bitmask for PIDs 0x21-0x40
  // etc.
  const discoveryCommands = [
    ["0100", 0x01],  // PIDs 0x01-0x20
    ["0120", 0x21],  // PIDs 0x21-0x40
    ["0140", 0x41],  // PIDs 0x41-0x60
    ["0160", 0x61],  // PIDs 0x61-0x80
    ["0180", 0x81],  // PIDs 0x81-0xA0
    ["01A0", 0xA1],  // PIDs 0xA1-0xC0
  ];

  // Ensure we're using the functional broadcast header
  await elm.ensureHeader(serial, "18DB", "33F1");

  for (const [discoveryCmd, pidBase] of discoveryCommands) {
    try {
      const responseText = await serial.sendCommand(discoveryCmd);
      const bytes = parseResponseBytes(responseText);
      if (!bytes) {
        continue; // NO DATA or malformed
      }
      
      const dataBytes = stripEcho(bytes);
      if (!dataBytes || dataBytes.length < 4) {
        continue; // Not enough data
      }

      // Parse the bitmask to find supported PIDs in this range
      const pidsInRange = parsePidBitmask(dataBytes, pidBase);
      for (const pid of pidsInRange) {
        const pidHex = pidToHex(pid);
        supportedPids.set(pidHex, pid);
      }
    } catch (err) {
      console.warn(`PID discovery failed for ${discoveryCmd}:`, err);
      // Continue with next discovery command
    }
  }

  console.log(`Discovered ${supportedPids.size} supported PIDs:`, Array.from(supportedPids.keys()).join(", "));
  return supportedPids;
}

/**
 * Discovers which custom commands (non-Mode-01) actually return data.
 * Tests each command once and classifies as working or NO DATA.
 * Returns { working: Map<command, true>, noData: Map<command, metadata> }
 * where metadata tracks backoff attempts.
 */
export async function discoverCustomCommands(serial, elm, commands) {
  const working = new Map(); // command -> true
  const noData = new Map(); // command -> metadata
  
  for (const command of commands) {
    if (command.cmd?.["01"]) {
      // Skip Mode 01 commands (handled by discoverSupportedPids)
      working.set(command, true);
      continue;
    }

    try {
      const hdr = command.hdr;
      const rax = command.rax;
      const [[modeHex, pidHex]] = Object.entries(command.cmd);
      
      // Set header for this command
      await elm.ensureHeader(serial, hdr, rax);
      
      // Send the command
      const responseText = await serial.sendCommand(`${modeHex}${pidHex}`);
      const firstLine = responseText.split("\r")[0].trim();
      
      if (ERROR_RESPONSE.test(firstLine)) {
        // NO DATA response
        noData.set(command, {
          failCount: 1,
          lastRetry: Date.now(),
          nextRetry: Date.now() + 10000, // 10 seconds
        });
      } else {
        // Has data
        working.set(command, true);
      }
    } catch (err) {
      // Treat network errors as NO DATA for now
      noData.set(command, {
        failCount: 1,
        lastRetry: Date.now(),
        nextRetry: Date.now() + 10000,
      });
    }
  }

  const workingCount = working.size;
  const noDataCount = noData.size;
  console.log(`Custom PID discovery: ${workingCount} working, ${noDataCount} returning NO DATA`);
  
  return { working, noData };
}

