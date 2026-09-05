// Web Serial transport: opens the port, frames responses on the ELM327
// `>` prompt, and enforces one command in flight at a time. Every other
// module reaches the port only through sendCommand()/onEvent() here.

// Bluetooth SPP virtual COM ports ignore the requested baud rate (the link
// is already framed over RFCOMM), but Web Serial's open() still requires one.
const BAUD_RATE = 115200;
const PROMPT = ">";

let port = null;
let reader = null;
let writer = null;
let readLoopPromise = null;
let rxBuffer = "";
const requestQueue = [];
let current = null; // {text, resolve, reject} for the in-flight command
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
  port = await navigator.serial.requestPort();
  await port.open({ baudRate: BAUD_RATE });
  rxBuffer = "";
  writer = port.writable.getWriter();
  readLoopPromise = readLoop();
}

export async function tryReconnect() {
  const ports = await navigator.serial.getPorts();
  if (ports.length === 0) {
    return false;
  }
  port = ports[0];
  await port.open({ baudRate: BAUD_RATE });
  rxBuffer = "";
  writer = port.writable.getWriter();
  readLoopPromise = readLoop();
  return true;
}

export function sendCommand(text) {
  return new Promise((resolve, reject) => {
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
}

export function isConnected() {
  return port !== null;
}
