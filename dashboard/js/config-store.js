// localStorage-backed signal-profile storage. Seeds the bundled SAEJ1979
// profile on first load; the only module that talks to localStorage.

const STORAGE_KEY = "esp-obd-dashboard:profiles";
const BUILTIN_ID = "saej1979";
const BUILTIN_NAME = "SAEJ1979 (Generic OBD-II)";

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

// Top-level await: callers that `await import('./config-store.js')` (or
// chain `.then()` off a dynamic import) only see this module once seeding
// has finished, so the exports below can stay synchronous.
let store = readStore();
if (!store) {
  const builtinData = await fetchBuiltinProfile();
  store = {
    profiles: {
      [BUILTIN_ID]: { name: BUILTIN_NAME, isBuiltin: true, data: builtinData },
    },
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
  if (store.activeId === id) {
    store.activeId = BUILTIN_ID;
  }
  writeStore(store);
}
