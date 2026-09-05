// Manages discovery state for custom PIDs with backoff timers.
// Persists state to localStorage and tracks NO DATA commands across sessions.

const STORAGE_KEY = "esp-obd-dashboard:discovery-state";
const SHOW_NO_DATA_KEY = "esp-obd-dashboard:show-no-data";

// Backoff schedule in milliseconds: [10s, 30s, 1m, 5m, infinity]
const BACKOFF_SCHEDULE = [10000, 30000, 60000, 300000];

let state = {}; // Map of profileId -> { working: Set, noData: Map }
let showNoData = false; // User's preference for showing NO DATA commands
const listeners = new Set();

/**
 * Load discovery state from localStorage.
 */
function loadFromStorage() {
  try {
    const stored = localStorage.getItem(STORAGE_KEY);
    if (stored) {
      state = JSON.parse(stored);
    } else {
      state = {};
    }
  } catch (err) {
    console.error("Failed to load discovery state from localStorage:", err);
    state = {};
  }

  try {
    showNoData = localStorage.getItem(SHOW_NO_DATA_KEY) === "true";
  } catch (err) {
    showNoData = false;
  }
}

/**
 * Save discovery state to localStorage.
 */
function saveToStorage() {
  try {
    // Convert Sets and Maps to JSON-serializable format
    const toSave = {};
    for (const [profileId, data] of Object.entries(state)) {
      toSave[profileId] = {
        working: Array.from(data.working || []),
        noData: Array.from((data.noData || new Map()).entries()),
      };
    }
    localStorage.setItem(STORAGE_KEY, JSON.stringify(toSave));
  } catch (err) {
    console.error("Failed to save discovery state to localStorage:", err);
  }
}

/**
 * Initialize discovery state for a profile.
 */
export function initProfile(profileId, commands) {
  loadFromStorage();

  if (!state[profileId]) {
    state[profileId] = {
      working: new Set(),
      noData: new Map(),
    };
  }

  // Restore Sets/Maps from storage format
  if (Array.isArray(state[profileId].working)) {
    state[profileId].working = new Set(state[profileId].working);
  }
  if (Array.isArray(state[profileId].noData)) {
    state[profileId].noData = new Map(state[profileId].noData);
  }

  // Ensure we have proper structure
  if (!(state[profileId].working instanceof Set)) {
    state[profileId].working = new Set();
  }
  if (!(state[profileId].noData instanceof Map)) {
    state[profileId].noData = new Map();
  }
}

/**
 * Record discovery results for a profile.
 */
export function recordDiscovery(profileId, working, noData) {
  initProfile(profileId, []);
  state[profileId].working = working;
  state[profileId].noData = noData;
  saveToStorage();
  emit({ type: "discovery-updated", profileId });
}

/**
 * Get discovery state for a profile.
 */
export function getState(profileId) {
  initProfile(profileId, []);
  return state[profileId];
}

/**
 * Check if a command should be retried based on its backoff state.
 * Returns true if the command should be tested now, false otherwise.
 */
export function shouldRetry(profileId, command) {
  initProfile(profileId, []);
  const noDataMap = state[profileId].noData;
  
  if (!noDataMap.has(command)) {
    return false; // Not in NO DATA list
  }

  const metadata = noDataMap.get(command);
  if (metadata.failCount >= BACKOFF_SCHEDULE.length + 1) {
    return false; // Permanently failed
  }

  return Date.now() >= metadata.nextRetry;
}

/**
 * Record a retry attempt for a NO DATA command.
 */
export function recordRetry(profileId, command) {
  initProfile(profileId, []);
  const noDataMap = state[profileId].noData;

  if (!noDataMap.has(command)) {
    return; // Not tracked
  }

  const metadata = noDataMap.get(command);
  const failCount = metadata.failCount + 1;

  if (failCount > BACKOFF_SCHEDULE.length) {
    // Permanently failed - remove from NO DATA list
    noDataMap.delete(command);
  } else {
    // Update with next backoff window
    const backoffMs = BACKOFF_SCHEDULE[failCount - 1];
    noDataMap.set(command, {
      failCount,
      lastRetry: Date.now(),
      nextRetry: Date.now() + backoffMs,
    });
  }

  saveToStorage();
  emit({ type: "backoff-updated", profileId, command });
}

/**
 * Record that a NO DATA command is now working.
 */
export function recordSuccess(profileId, command) {
  initProfile(profileId, []);
  const noDataMap = state[profileId].noData;

  if (noDataMap.has(command)) {
    noDataMap.delete(command);
    state[profileId].working.add(command);
    saveToStorage();
    emit({ type: "command-recovered", profileId, command });
  }
}

/**
 * Clear discovery state for a profile (e.g., on profile switch).
 */
export function clearProfile(profileId) {
  delete state[profileId];
  saveToStorage();
}

/**
 * Set the "show NO DATA" preference.
 */
export function setShowNoData(value) {
  showNoData = value;
  try {
    localStorage.setItem(SHOW_NO_DATA_KEY, String(value));
  } catch (err) {
    console.error("Failed to save show-no-data preference:", err);
  }
  emit({ type: "show-no-data-changed", showNoData: value });
}

/**
 * Get the "show NO DATA" preference.
 */
export function getShowNoData() {
  return showNoData;
}

/**
 * Listen for discovery state changes.
 */
export function onChange(callback) {
  listeners.add(callback);
  return () => listeners.delete(callback);
}

function emit(event) {
  for (const listener of listeners) {
    listener(event);
  }
}

// Initialize on module load
loadFromStorage();
