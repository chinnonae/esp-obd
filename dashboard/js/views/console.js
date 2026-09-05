// Raw TX/RX log: subscribes directly to serial.js/simulator.js's onEvent,
// independent of the poll engine or decoder, so it stays useful even when
// nothing decodes. Ring-buffers the last MAX_ENTRIES lines.

import * as serial from "../serial.js";
import * as simulator from "../simulator.js";

const MAX_ENTRIES = 1000;
const SCROLL_BOTTOM_THRESHOLD_PX = 16;

let entries = [];
let listEl = null;
let userScrolledUp = false;

function formatTimestamp(ts) {
  const date = new Date(ts);
  const time = date.toLocaleTimeString("en-US", { hour12: false });
  const millis = String(date.getMilliseconds()).padStart(3, "0");
  return `${time}.${millis}`;
}

function isNearBottom() {
  return listEl.scrollTop + listEl.clientHeight >= listEl.scrollHeight - SCROLL_BOTTOM_THRESHOLD_PX;
}

function appendEntry(event) {
  entries.push(event);
  if (entries.length > MAX_ENTRIES) {
    entries.shift();
    listEl.firstElementChild?.remove();
  }

  const line = document.createElement("div");
  
  // Handle different event types
  if (event.type === "protocol") {
    line.className = "console-line console-line--info";
    if (event.protocol === null) {
      line.textContent = `[${formatTimestamp(event.timestamp)}] Protocol disconnected`;
    } else if (event.protocol === "Auto-search") {
      line.textContent = `[${formatTimestamp(event.timestamp)}] Protocol searching...`;
    } else {
      line.textContent = `[${formatTimestamp(event.timestamp)}] Protocol: ${event.protocol}`;
    }
  } else {
    // TX/RX events
    line.className = `console-line console-line--${event.direction}`;
    const prefix = event.direction === "tx" ? "TX" : "RX";
    line.textContent = `[${formatTimestamp(event.timestamp)}] ${prefix} ${event.text}`;
  }
  
  listEl.appendChild(line);

  if (!userScrolledUp) {
    listEl.scrollTop = listEl.scrollHeight;
  }
}

export function init() {
  const root = document.getElementById("view-console");
  root.innerHTML = "";

  const toolbar = document.createElement("div");
  toolbar.className = "console-toolbar";
  const clearButton = document.createElement("button");
  clearButton.type = "button";
  clearButton.textContent = "Clear";
  toolbar.appendChild(clearButton);

  listEl = document.createElement("div");
  listEl.className = "console-log";

  root.append(toolbar, listEl);

  clearButton.addEventListener("click", () => {
    entries = [];
    listEl.innerHTML = "";
    userScrolledUp = false;
  });

  listEl.addEventListener("scroll", () => {
    userScrolledUp = !isNearBottom();
  });

  // Whichever transport (real or simulator) is actually connected is the
  // only one that will ever emit -- subscribing to both keeps this view
  // decoupled from app.js's transport choice.
  serial.onEvent(appendEntry);
  simulator.onEvent(appendEntry);
}
