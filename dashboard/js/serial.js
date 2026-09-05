// Web Serial transport: opens the port, frames responses on the ELM327
// `>` prompt, and enforces one command in flight at a time. Every other
// module reaches the port only through sendCommand()/onEvent() here.

// Bluetooth SPP virtual COM ports ignore the requested baud rate (the link
// is already framed over RFCOMM), but Web Serial's open() still requires one.
const BAUD_RATE = 115200;
const PROMPT = ">";
// port.open() can hang indefinitely if the paired device isn't actually
// connected/awake right now (observed against real ESP-OBD hardware) --
// give up cleanly instead of leaving the caller stuck forever.
const OPEN_TIMEOUT_MS = 8000;

let port = null;
let reader = null;
let writer = null;
let readLoopPromise = null;
let rxBuffer = "";
let opening = false; // guards against connect()/tryReconnect() racing each other
const requestQueue = [];
let current = null; // {text, resolve, reject} for the in-flight command
const listeners = new Set();
let currentProtocol = null; // current protocol state

function emit(event) {
  for (const listener of listeners) {
    listener(event);
  }
}

export function onEvent(callback) {
  listeners.add(callback);
  return () => listeners.delete(callback);
}

function pumpQueue() {
  if (current || requestQueue.length === 0) {
    return;
  }
  current = requestQueue.shift();
  emit({ direction: "tx", text: current.text, timestamp: Date.now() });
  const encoder = new TextEncoder();
  writer.write(encoder.encode(`${current.text}\r`)).catch((err) => {
    const failed = current;
    current = null;
    failed.reject(err);
    pumpQueue();
  });
}

function resolveCurrent(responseText) {
  const finished = current;
  current = null;
  if (finished) {
    finished.resolve(responseText);
  }
  pumpQueue();
}

function rejectAll(error) {
  if (current) {
    current.reject(error);
    current = null;
  }
  while (requestQueue.length > 0) {
    requestQueue.shift().reject(error);
  }
}

async function readLoop() {
  const textDecoder = new TextDecoderStream();
  const readableClosed = port.readable.pipeTo(textDecoder.writable);
  reader = textDecoder.readable.getReader();

  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) {
        break;
      }
      rxBuffer += value;

      let promptIndex;
      while ((promptIndex = rxBuffer.indexOf(PROMPT)) !== -1) {
        const rawFrame = rxBuffer.slice(0, promptIndex);
        rxBuffer = rxBuffer.slice(promptIndex + 1);

        const text = rawFrame.replace(/\r+/g, "\r").split("\r").filter(Boolean).join("\r");
        emit({ direction: "rx", text, timestamp: Date.now() });
        resolveCurrent(text);
      }
    }
  } catch (err) {
    rejectAll(err);
  } finally {
    reader.releaseLock();
    await readableClosed.catch(() => {});
  }
}

export async function connect() {
  if (opening || port) {
    throw new Error("A serial connection is already open or connecting");
  }
  opening = true;
  try {
    const requested = await navigator.serial.requestPort();
    await openPort(requested);
  } finally {
    opening = false;
  }
}

export async function tryReconnect() {
  if (opening || port) {
    throw new Error("A serial connection is already open or connecting");
  }
  opening = true;
  try {
    const ports = await navigator.serial.getPorts();
    if (ports.length === 0) {
      return false;
    }
    await openPort(ports[0]);
    return true;
  } finally {
    opening = false;
  }
}

async function openPort(portToOpen) {
  port = portToOpen;
  try {
    await withTimeout(
      port.open({ baudRate: BAUD_RATE }),
      OPEN_TIMEOUT_MS,
      "Timed out opening the serial port -- is the device connected/awake?"
    );
  } catch (err) {
    port = null;
    throw err;
  }
  rxBuffer = "";
  writer = port.writable.getWriter();
  readLoopPromise = readLoop();
}

function withTimeout(promise, ms, message) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(message)), ms);
    promise.then(
      (value) => {
        clearTimeout(timer);
        resolve(value);
      },
      (err) => {
        clearTimeout(timer);
        reject(err);
      }
    );
  });
}

export function sendCommand(text) {
  return new Promise((resolve, reject) => {
    // Check for protocol-setting commands and emit protocol events
    const protocolMatch = text.match(/^(?:ATP|ATSP)([0-9A-Fa-f])/i);
    if (protocolMatch) {
      const protocolCode = protocolMatch[1].toUpperCase();
      const PROTOCOL_MAP = {
        "0": "Auto-search",
        "6": "11-bit 500k",
        "7": "29-bit 500k",
        "8": "11-bit 250k",
        "9": "29-bit 250k",
      };
      const protocolName = PROTOCOL_MAP[protocolCode] || `Unknown (${protocolCode})`;
      if (protocolName !== currentProtocol) {
        currentProtocol = protocolName;
        emit({ type: "protocol", protocol: protocolName, timestamp: Date.now() });
      }
    }
    
    requestQueue.push({ text, resolve, reject });
    pumpQueue();
  });
}

export async function disconnect() {
  rejectAll(new Error("Disconnected"));
  if (reader) {
    await reader.cancel().catch(() => {});
  }
  if (readLoopPromise) {
    await readLoopPromise.catch(() => {});
  }
  if (writer) {
    writer.releaseLock();
    writer = null;
  }
  if (port) {
    await port.close().catch(() => {});
    port = null;
  }
  // Clear protocol state on disconnect
  if (currentProtocol !== null) {
    currentProtocol = null;
    emit({ type: "protocol", protocol: null, timestamp: Date.now() });
  }
}

export function isConnected() {
  return port !== null;
}

/**
 * Set the current protocol and emit a protocol-change event.
 * Called by elm.js when protocol selection commands are processed.
 */
export function setProtocol(protocolName) {
  if (protocolName !== currentProtocol) {
    currentProtocol = protocolName;
    emit({ type: "protocol", protocol: protocolName, timestamp: Date.now() });
  }
}
