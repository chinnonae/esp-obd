// localStorage-backed signal-profile storage. Seeds the bundled SAEJ1979,
// Honda, and Honda-Civic profiles on first load; the only module that talks
// to localStorage.

const STORAGE_KEY = "esp-obd-dashboard:profiles";
const BUILTIN_ID = "saej1979";
const BUILTIN_NAME = "SAEJ1979 (Generic OBD-II)";
const BUILTIN_ID_HONDA = "builtin:honda";
const BUILTIN_NAME_HONDA = "Honda (OBDb)";
const BUILTIN_ID_CIVIC = "builtin:civic";
const BUILTIN_NAME_CIVIC = "Honda Civic (OBDb)";

function readStore() {
  const raw = localStorage.getItem(STORAGE_KEY);
  if (!raw) {
    return null;
  }
  try {
    return JSON.parse(raw);
  } catch {
    return null;
  }
}

function writeStore(store) {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(store));
}

async function fetchBuiltinProfile() {
  const url = new URL("../data/saej1979.json", import.meta.url);
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Failed to fetch bundled profile: ${response.status}`);
  }
  return response.json();
}

async function fetchBuiltinProfiles() {
  const profiles = {};
  
  // Fetch SAEJ1979
  try {
    const saejUrl = new URL("../data/saej1979.json", import.meta.url);
    const saejResponse = await fetch(saejUrl);
    if (saejResponse.ok) {
      profiles[BUILTIN_ID] = {
        name: BUILTIN_NAME,
        isBuiltin: true,
        data: await saejResponse.json(),
      };
    }
  } catch (e) {
    console.warn(`Failed to fetch SAEJ1979 profile: ${e.message}`);
  }
  
  // Fetch Honda
  try {
    const hondaUrl = new URL("../data/honda.json", import.meta.url);
    const hondaResponse = await fetch(hondaUrl);
    if (hondaResponse.ok) {
      profiles[BUILTIN_ID_HONDA] = {
        name: BUILTIN_NAME_HONDA,
        isBuiltin: true,
        data: await hondaResponse.json(),
      };
    }
  } catch (e) {
    console.warn(`Failed to fetch Honda profile: ${e.message}`);
  }
  
  // Fetch Honda Civic
  try {
    const civicUrl = new URL("../data/honda-civic.json", import.meta.url);
    const civicResponse = await fetch(civicUrl);
    if (civicResponse.ok) {
      profiles[BUILTIN_ID_CIVIC] = {
        name: BUILTIN_NAME_CIVIC,
        isBuiltin: true,
        data: await civicResponse.json(),
      };
    }
  } catch (e) {
    console.warn(`Failed to fetch Honda Civic profile: ${e.message}`);
  }
  
  return profiles;
}

// Top-level await: callers that `await import('./config-store.js')` (or
// chain `.then()` off a dynamic import) only see this module once seeding
// has finished, so the exports below can stay synchronous.
let store = readStore();
if (!store) {
  const builtinProfiles = await fetchBuiltinProfiles();
  store = {
    profiles: builtinProfiles,
    activeId: BUILTIN_ID,
  };
  writeStore(store);
}

export function listProfiles() {
  return Object.entries(store.profiles).map(([id, profile]) => ({
    id,
    name: profile.name,
    isBuiltin: Boolean(profile.isBuiltin),
  }));
}

const activeProfileChangeListeners = new Set();

export function onActiveProfileChange(callback) {
  activeProfileChangeListeners.add(callback);
  return () => activeProfileChangeListeners.delete(callback);
}

export function getProfile(id) {
  return store.profiles[id]?.data;
}

export function getActiveProfileId() {
  return store.activeId;
}

export function setActiveProfileId(id) {
  if (!store.profiles[id]) {
    throw new Error(`Unknown profile id: ${id}`);
  }
  store.activeId = id;
  writeStore(store);
  for (const listener of activeProfileChangeListeners) {
    listener(id);
  }
}

export function saveUploadedProfile(name, json) {
  if (!json || !Array.isArray(json.commands)) {
    return { error: "Profile must have a 'commands' array." };
  }
  const id = `uploaded:${name}`;
  store.profiles[id] = { name, isBuiltin: false, data: json };
  writeStore(store);
  return { id };
}

export function deleteProfile(id) {
  const profile = store.profiles[id];
  if (!profile) {
    return;
  }
  if (profile.isBuiltin) {
    throw new Error("Cannot delete the builtin profile.");
  }
  delete store.profiles[id];
  const activeChanged = store.activeId === id;
  if (activeChanged) {
    store.activeId = BUILTIN_ID;
  }
  writeStore(store);
  if (activeChanged) {
    for (const listener of activeProfileChangeListeners) {
      listener(store.activeId);
    }
  }
}
