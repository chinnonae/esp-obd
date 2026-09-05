// Round-robins the active profile's commands over the connected session,
// decodes each response, and publishes signal-update events for the
// Current/Timeseries tabs. No DOM/localStorage access of its own.

import { decodeResponse, computeSynthetics } from "./decoder.js";
import { discoverSupportedPids } from "./pid-discovery.js";

const ERROR_RESPONSE = /^(NO DATA|UNABLE TO CONNECT|ERROR|BUS INIT|STOPPED|\?)/i;

let running = false;
let lastHdrRax = null; // (hdr, rax) of the last command sent, across passes
let lastPassMs = 0;
let supportedPids = new Set(); // Set of supported PID hex strings discovered at startup
const listeners = new Set();

export function onUpdate(callback) {
  listeners.add(callback);
  return () => listeners.delete(callback);
}

function emit(update) {
  for (const listener of listeners) {
    listener(update);
  }
}

function parseResponseBytes(text) {
  // Multi-ECU/multi-line responses: take the first line only (see D05 notes).
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

function stripEcho(bytes, modeHex, pidHex) {
  const expectedModeByte = parseInt(modeHex, 16) + 0x40;
  if (bytes.length === 0 || bytes[0] !== expectedModeByte) {
    return null;
  }
  const pidByteLen = pidHex.length / 2;
  return bytes.slice(1 + pidByteLen);
}

async function pollOnePass(serial, elm, profile) {
  const signalValues = {};

  for (const command of profile.commands) {
    const [[modeHex, pidHex]] = Object.entries(command.cmd);
    
    // Skip Mode 01 PIDs that are not supported
    if (modeHex === "01" && !supportedPids.has(pidHex)) {
      continue;
    }
    
    const hdrRax = `${command.hdr}:${command.rax}`;
    if (hdrRax !== lastHdrRax) {
      await elm.ensureHeader(serial, command.hdr, command.rax);
      lastHdrRax = hdrRax;
    }

    let responseText;
    try {
      responseText = await serial.sendCommand(`${modeHex}${pidHex}`);
    } catch (err) {
      // Transport failure (e.g. disconnected mid-pass) -- abandon this pass
      // rather than hammer more requests at a dead connection.
      if (typeof serial.isConnected === "function" && !serial.isConnected()) {
        stop();
      }
      return;
    }

    const bytes = parseResponseBytes(responseText);
    if (!bytes) {
      continue; // NO DATA / ? / malformed -- no update this round
    }
    const dataBytes = stripEcho(bytes, modeHex, pidHex);
    if (!dataBytes) {
      continue;
    }

    const decoded = decodeResponse(command, dataBytes);
    const timestamp = Date.now();
    for (const [signalId, value] of Object.entries(decoded)) {
      if (value === undefined) {
        continue;
      }
      signalValues[signalId] = value;
      emit({ signalId, value, timestamp });
    }
  }

  const synthetics = computeSynthetics(signalValues, profile.synthetics);
  const timestamp = Date.now();
  for (const [signalId, value] of Object.entries(synthetics)) {
    emit({ signalId, value, timestamp });
  }
}

async function runLoop(serial, elm, configStore) {
  // Discovery phase: run once at startup
  console.log("Starting Mode 01 PID discovery...");
  try {
    const discovered = await discoverSupportedPids(serial, elm);
    supportedPids = discovered;
  } catch (err) {
    console.error("PID discovery failed, will poll all commands anyway:", err);
    // If discovery fails, assume all PIDs are supported (fallback)
    supportedPids = new Set(["00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B", "0C", "0D", "0E", "0F", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "1A", "1B", "1C", "1D", "1E", "1F", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "2A", "2B", "2C", "2D", "2E", "2F", "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3A", "3B", "3C", "3D", "3E", "3F", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4A", "4B", "4C", "4D", "4E", "4F", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "5A", "5B", "5C", "5D", "5E", "5F", "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6A", "6B", "6C", "6D", "6E", "6F", "70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "7A", "7B", "7C", "7D", "7E", "7F", "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F", "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9A", "9B", "9C", "9D", "9E", "9F", "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF", "B0"]);
  }
  
  lastHdrRax = null;

  // Main polling loop
  while (running) {
    const passStart = Date.now();
    const profile = configStore.getProfile(configStore.getActiveProfileId());
    if (profile && Array.isArray(profile.commands)) {
      await pollOnePass(serial, elm, profile);
    }
    lastPassMs = Date.now() - passStart;
  }
}

export function start(serial, elm, configStore) {
  if (running) {
    return;
  }
  running = true;
  lastHdrRax = null;
  supportedPids = new Set(); // Reset discovered PIDs
  runLoop(serial, elm, configStore);
}

export function stop() {
  running = false;
}

export function isRunning() {
  return running;
}

export function getLastPassMs() {
  return lastPassMs;
}
