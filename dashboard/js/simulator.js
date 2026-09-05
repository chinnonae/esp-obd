// Fake ELM327 + vehicle responder, matching serial.js's interface
// (connect/tryReconnect/sendCommand/onEvent/disconnect/isConnected) so
// elm.js and anything built on top of it can't tell the difference. For
// local development/testing without the real ESP-OBD adapter.

const RESPONSE_DELAY_MS = 40;

let connected = false;
let startTime = 0;
const listeners = new Set();

function emit(event) {
  for (const listener of listeners) {
    listener(event);
  }
}

export function onEvent(callback) {
  listeners.add(callback);
  return () => listeners.delete(callback);
}

function hex2(n) {
  return (n & 0xff).toString(16).toUpperCase().padStart(2, "0");
}

function elapsedSeconds() {
  return (Date.now() - startTime) / 1000;
}

// Slowly-varying fake values for a handful of common Mode 01 PIDs, keyed by
// PID hex string. Returns the response's data-byte hex string, or null for
// an unsupported PID (-> NO DATA).
function fakeDataForPid(pid) {
  const t = elapsedSeconds();
  switch (pid) {
    case "00":
      return "98 3B 80 13"; // fake "supported PIDs 01-20" bitmap
    case "04": { // calculated engine load, percent
      const load = 30 + 20 * Math.sin(t / 7);
      return hex2(Math.round((load * 255) / 100));
    }
    case "05": { // engine coolant temp, celsius
      const celsius = 85 + 5 * Math.sin(t / 20);
      return hex2(Math.round(celsius) + 40);
    }
    case "0C": { // engine RPM
      const rpm = 800 + 700 * (1 + Math.sin(t / 3));
      const raw = Math.round(rpm * 4);
      return `${hex2(raw >> 8)} ${hex2(raw)}`;
    }
    case "0D": { // vehicle speed, km/h
      const speed = Math.max(0, 40 + 40 * Math.sin(t / 10));
      return hex2(Math.round(speed));
    }
    case "0F": // intake air temp, celsius
      return hex2(25 + 40);
    case "11": { // absolute throttle position, percent
      const pos = Math.max(0, Math.min(100, 20 + 15 * Math.sin(t / 5)));
      return hex2(Math.round((pos * 255) / 100));
    }
    case "2F": // fuel tank level, percent
      return hex2(Math.round((60 * 255) / 100));
    default:
      return null;
  }
}

function handleCommand(text) {
  const upper = text.trim().toUpperCase();

  if (upper === "ATZ") {
    return "ELM327 v1.5 (simulator)";
  }
  if (upper.startsWith("AT")) {
    return "OK";
  }

  const match = upper.match(/^01([0-9A-F]{2})$/);
  if (match) {
    const pid = match[1];
    const data = fakeDataForPid(pid);
    return data ? `41 ${pid} ${data}` : "NO DATA";
  }

  return "NO DATA";
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export async function connect() {
  await delay(150); // mimic the real port-open latency
  connected = true;
  startTime = Date.now();
}

export async function tryReconnect() {
  return false; // no persisted virtual port to silently reopen
}

export async function sendCommand(text) {
  if (!connected) {
    throw new Error("Simulator not connected");
  }
  emit({ direction: "tx", text, timestamp: Date.now() });
  await delay(RESPONSE_DELAY_MS);
  const responseText = handleCommand(text);
  emit({ direction: "rx", text: responseText, timestamp: Date.now() });
  return responseText;
}

export async function disconnect() {
  connected = false;
}

export function isConnected() {
  return connected;
}
