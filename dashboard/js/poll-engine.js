// Round-robins the active profile's commands over the connected session,
// decodes each response, and publishes signal-update events for the
// Current/Timeseries tabs. No DOM/localStorage access of its own.

import { decodeResponse, computeSynthetics } from "./decoder.js";

const ERROR_RESPONSE = /^(NO DATA|UNABLE TO CONNECT|ERROR|BUS INIT|STOPPED|\?)/i;

let running = false;
let lastHdrRax = null; // (hdr, rax) of the last command sent, across passes
let lastPassMs = 0;
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
