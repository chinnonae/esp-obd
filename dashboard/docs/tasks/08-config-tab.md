# D08 - Config Tab

**Status:** Done (2026-09-05)
**Depends on:** [D02](02-config-store-and-bundled-profile.md)

## Goal

UI for uploading a vehicle-specific OBDb signalset file, switching the
active profile, and deleting uploaded profiles — a thin wrapper over
`config-store.js`.

## Scope

- `dashboard/js/views/config.js`:
  - A file input (`<input type="file" accept=".json">`) that reads the
    selected file as text, `JSON.parse`s it, and calls
    `configStore.saveUploadedProfile(name, json)`; shows a clear error if
    validation fails (not just a silent no-op).
  - A list of stored profiles (builtin + uploaded), each showing its name,
    a "make active" control, and (for non-builtin) a delete button.
  - Highlights whichever profile is currently active.
  - An attribution footer/link for the bundled SAEJ1979 profile
    (CC BY-SA 4.0, link to `github.com/OBDb/SAEJ1979`).

## Steps

1. Wire the file input to read + parse + validate + `saveUploadedProfile`.
2. Render the profile list from `configStore.listProfiles()`, re-rendering
   on any change (upload, delete, active-selection change).
3. Wire "make active" to `configStore.setActiveProfileId`.
4. Wire delete, disabled/hidden for the builtin profile.
5. Add the attribution footer.

## Acceptance criteria

- Uploading a real OBDb `default.json` (e.g. from `OBDb/Nissan-Leaf`) adds
  it to the list and it can be made active.
- Uploading a non-signalset JSON file (missing `commands`) shows an error
  and does not corrupt the stored profile list.
- Making a profile active is immediately reflected by whichever profile
  [D05](05-poll-engine.md)'s poll engine is using next pass.
- The builtin profile cannot be deleted from this UI.

## Verification

Manual, in-browser: download a real vehicle's `signalsets/v3/default.json`
from `github.com/OBDb`, upload it here, switch to it, confirm the Current
tab ([D06](06-current-tab.md)) now shows that vehicle's signals.

## Notes

- This view only talks to `config-store.js` — it doesn't touch the serial
  port or the poll engine directly, matching the architecture's "views only
  subscribe" rule.
- Verified 2026-09-05: an invalid upload (missing `commands`) was rejected
  with a clear error and didn't touch the stored profile list; a real
  vehicle signalset (`OBDb/Nissan-Leaf`'s `signalsets/v3/default.json`, 29
  mode-22 commands with `fcm1`/multi-byte `bix` fields) uploaded
  successfully through the actual file-input path, appeared in the list,
  and could be made active. Activating a small hand-built profile was
  immediately reflected on the Current tab (D06) via the simulator, next
  poll-engine pass, no reconnect. Delete removed a non-builtin profile and
  fell back the active id to the builtin when the active one was deleted;
  the builtin has no delete button.
- The Nissan-Leaf profile's mode-22 signals didn't produce Current-tab
  tiles in this session because neither the simulator (Mode 01 only) nor
  the bench unit (no real Leaf attached) answer those PIDs — the
  upload/activate/poll-engine plumbing was confirmed, but decoding real
  mode-22 responses end-to-end still needs a real vehicle or simulator
  support for it (tracked as a general gap, not specific to this task).
