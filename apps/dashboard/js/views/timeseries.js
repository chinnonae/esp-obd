// Rolling per-signal history + a hand-rolled canvas line chart (no
// charting library, per DESIGN_DECISIONS.md). The buffer subscription runs
// at module load time, independent of init()/tab visibility, so history
// keeps collecting in the background regardless of which tab is active.

import * as pollEngine from "../poll-engine.js";

const WINDOW_OPTIONS = [
  { label: "1 min", ms: 60 * 1000 },
  { label: "5 min", ms: 5 * 60 * 1000 },
  { label: "15 min", ms: 15 * 60 * 1000 },
];
const MAX_BUFFER_MS = WINDOW_OPTIONS[WINDOW_OPTIONS.length - 1].ms;

const buffers = {}; // signalId -> [{timestamp, value}], oldest first
let knownIds = [];
let selectedSignal = null;
let windowMs = WINDOW_OPTIONS[0].ms;

let selectEl = null;
let windowSelectEl = null;
let canvas = null;
let ctx = null;

function recordSample(signalId, value, timestamp) {
  if (typeof value !== "number") {
    return; // only numeric signals are chartable
  }
  const buf = buffers[signalId] ?? (buffers[signalId] = []);
  buf.push({ timestamp, value });
  const cutoff = timestamp - MAX_BUFFER_MS;
  while (buf.length > 0 && buf[0].timestamp < cutoff) {
    buf.shift();
  }
}

pollEngine.onUpdate(({ signalId, value, timestamp }) => {
  recordSample(signalId, value, timestamp);
});

function refreshSignalOptions() {
  const ids = Object.keys(buffers).sort();
  const unchanged = ids.length === knownIds.length && ids.every((id, i) => id === knownIds[i]);
  if (unchanged) {
    return;
  }
  knownIds = ids;

  const previous = selectEl.value;
  selectEl.innerHTML = "";
  for (const id of ids) {
    const option = document.createElement("option");
    option.value = id;
    option.textContent = id;
    selectEl.appendChild(option);
  }
  selectEl.value = ids.includes(previous) ? previous : (ids[0] ?? "");
  selectedSignal = selectEl.value || null;
}

function draw() {
  const width = canvas.width;
  const height = canvas.height;
  ctx.clearRect(0, 0, width, height);

  const paddingLeft = 44;
  const paddingBottom = 20;
  const paddingTop = 10;
  const paddingRight = 10;
  const plotWidth = width - paddingLeft - paddingRight;
  const plotHeight = height - paddingTop - paddingBottom;

  ctx.fillStyle = "#8b98a5";
  ctx.font = "11px sans-serif";

  const buffer = selectedSignal ? buffers[selectedSignal] : undefined;
  const now = Date.now();
  const from = now - windowMs;
  const samples = buffer ? buffer.filter((s) => s.timestamp >= from) : [];

  if (samples.length === 0) {
    ctx.fillText(selectedSignal ? "No data in this window" : "No signals yet", paddingLeft, paddingTop + 12);
    return;
  }

  let min = Math.min(...samples.map((s) => s.value));
  let max = Math.max(...samples.map((s) => s.value));
  if (min === max) {
    min -= 1;
    max += 1;
  }

  ctx.strokeStyle = "#2a323c";
  const gridLines = 4;
  for (let i = 0; i <= gridLines; i++) {
    const y = paddingTop + (plotHeight * i) / gridLines;
    ctx.beginPath();
    ctx.moveTo(paddingLeft, y);
    ctx.lineTo(width - paddingRight, y);
    ctx.stroke();
    const value = max - ((max - min) * i) / gridLines;
    ctx.fillText(value.toFixed(1), 2, y + 3);
  }

  ctx.strokeStyle = "#3ba7ff";
  ctx.lineWidth = 2;
  ctx.beginPath();
  samples.forEach((sample, i) => {
    const x = paddingLeft + ((sample.timestamp - from) / windowMs) * plotWidth;
    const y = paddingTop + plotHeight - ((sample.value - min) / (max - min)) * plotHeight;
    if (i === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  });
  ctx.stroke();
}

function tick() {
  refreshSignalOptions();
  draw();
}

export function init() {
  const root = document.getElementById("view-timeseries");
  root.innerHTML = "";

  const controls = document.createElement("div");
  controls.className = "timeseries-controls";

  selectEl = document.createElement("select");
  selectEl.addEventListener("change", () => {
    selectedSignal = selectEl.value || null;
    draw();
  });

  windowSelectEl = document.createElement("select");
  for (const option of WINDOW_OPTIONS) {
    const el = document.createElement("option");
    el.value = String(option.ms);
    el.textContent = option.label;
    windowSelectEl.appendChild(el);
  }
  windowSelectEl.value = String(windowMs);
  windowSelectEl.addEventListener("change", () => {
    windowMs = Number(windowSelectEl.value);
    draw();
  });

  controls.append(selectEl, windowSelectEl);

  canvas = document.createElement("canvas");
  canvas.className = "timeseries-canvas";
  canvas.width = 640;
  canvas.height = 300;
  ctx = canvas.getContext("2d");

  root.append(controls, canvas);

  tick();
  setInterval(tick, 1000);
}
