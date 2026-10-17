// Live tile grid: one tile per signal seen from the poll engine, grouped by
// the OBDb `path`'s top-level segment. No transport/storage access of its
// own -- reads signal metadata (name/unit/path) from config-store.

import * as configStore from "../config-store.js";
import * as pollEngine from "../poll-engine.js";

let signalMeta = {}; // signalId -> { name, unit, group }
let tiles = {}; // signalId -> { valueEl, unit }
let groupGrids = {}; // group label -> tile-grid element
let groupsContainer = null;
let statusEl = null;

function topLevelGroup(path) {
  return path ? path.split(".")[0] : "Other";
}

function buildSignalMeta(profile) {
  const meta = {};
  if (!profile || !Array.isArray(profile.commands)) {
    return meta;
  }
  for (const command of profile.commands) {
    for (const signal of command.signals ?? []) {
      meta[signal.id] = {
        name: signal.name ?? signal.id,
        unit: signal.fmt?.unit,
        group: topLevelGroup(signal.path),
      };
    }
  }
  return meta;
}

function ensureGroupGrid(groupLabel) {
  if (groupGrids[groupLabel]) {
    return groupGrids[groupLabel];
  }
  const section = document.createElement("section");
  section.className = "tile-group";
  const heading = document.createElement("h3");
  heading.textContent = groupLabel;
  const grid = document.createElement("div");
  grid.className = "tile-grid";
  section.append(heading, grid);
  groupsContainer.appendChild(section);
  groupGrids[groupLabel] = grid;
  return grid;
}

function formatValue(value, unit) {
  if (typeof value === "number") {
    const rounded = Number.isInteger(value) ? value : Math.round(value * 100) / 100;
    return unit ? `${rounded} ${unit}` : String(rounded);
  }
  return String(value);
}

function ensureTile(signalId) {
  const existing = tiles[signalId];
  if (existing) {
    return existing;
  }
  const meta = signalMeta[signalId] ?? { name: signalId, group: "Other" };
  const grid = ensureGroupGrid(meta.group);

  const tile = document.createElement("div");
  tile.className = "tile";
  const nameEl = document.createElement("div");
  nameEl.className = "tile-name";
  nameEl.textContent = meta.name;
  const valueEl = document.createElement("div");
  valueEl.className = "tile-value";
  valueEl.textContent = "--";
  tile.append(nameEl, valueEl);
  grid.appendChild(tile);

  const entry = { valueEl, unit: meta.unit };
  tiles[signalId] = entry;
  return entry;
}

function rebuild() {
  const profile = configStore.getProfile(configStore.getActiveProfileId());
  signalMeta = buildSignalMeta(profile);
  groupsContainer.innerHTML = "";
  groupGrids = {};
  tiles = {};
}

export function init() {
  const root = document.getElementById("view-current");
  root.innerHTML = "";

  statusEl = document.createElement("div");
  statusEl.className = "poll-status";
  groupsContainer = document.createElement("div");
  root.append(statusEl, groupsContainer);

  rebuild();
  configStore.onActiveProfileChange(rebuild);

  pollEngine.onUpdate(({ signalId, value }) => {
    const entry = ensureTile(signalId);
    entry.valueEl.textContent = formatValue(value, entry.unit);
  });

  setInterval(() => {
    statusEl.textContent = `Poll: ${pollEngine.getLastPassMs()} ms/pass`;
  }, 1000);
}
