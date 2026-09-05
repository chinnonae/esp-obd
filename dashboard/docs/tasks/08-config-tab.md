# D08 - Config Tab

**Status:** Planned
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
