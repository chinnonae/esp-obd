# D02 - Config Store and Bundled Generic Profile

**Status:** Done (2026-09-05)
**Depends on:** [D01](01-app-shell-and-pwa-scaffold.md)

## Goal

Persisted signal-profile storage backed by `localStorage`, seeded with a
bundled copy of OBDb's `SAEJ1979` generic Mode 01 signalset, so later tasks
(poll engine, Config tab) have something real to read from and write to.

## Background

Per [PLAN.md](../PLAN.md) and [DESIGN_DECISIONS.md](../DESIGN_DECISIONS.md#bundle-obdbs-own-saej1979-signalset-as-the-generic-mode-01-profile),
the generic profile is not hand-written — it's OBDb's own signalset, fetched
from `https://raw.githubusercontent.com/OBDb/SAEJ1979/main/signalsets/v3/default.json`
and committed verbatim.

## Scope

- `dashboard/data/saej1979.json` — the bundled file, with a leading
  attribution comment (or a sibling `saej1979.ATTRIBUTION.md`, since JSON
  has no comments) noting source URL, license (CC BY-SA 4.0), and fetch
  date.
- `dashboard/js/config-store.js` — exports:
  - `listProfiles()` — `{id, name, isBuiltin}[]`
  - `getProfile(id)` — the full signalset object
  - `getActiveProfileId()` / `setActiveProfileId(id)`
  - `saveUploadedProfile(name, json)` — validates minimally (`commands` is
    an array) and stores it
  - `deleteProfile(id)` — refuses to delete the builtin
  - On first call, seeds `localStorage` with the builtin profile from
    `data/saej1979.json` if nothing is stored yet.
- Add `data/saej1979.json` to `sw.js`'s precache list from D01.

## Steps

1. Fetch and commit `dashboard/data/saej1979.json` (already downloaded once
   during planning research — reuse or refetch to confirm it's current).
2. Write `config-store.js` with the API above, using a single
   `localStorage` key (e.g. `esp-obd-dashboard:profiles`) holding a small
   JSON index structure of `{profiles: {id: {name, isBuiltin, data}}, activeId}`.
3. Update `sw.js`'s precache list to include the new data file.

## Acceptance criteria

- On a fresh browser profile (cleared storage), loading the app seeds
  exactly one profile (the builtin) and marks it active.
- `saveUploadedProfile` rejects a file without a `commands` array (returns
  an error the Config tab can surface) rather than corrupting stored state.
- Reloading the page preserves whatever was active and any uploaded
  profiles — verified via DevTools > Application > Local Storage.
- `deleteProfile` on the builtin id is a no-op or throws, not silently
  destructive.

## Verification

Manual, via DevTools console against the served page:

```js
import('./js/config-store.js').then(m => {
  console.log(m.listProfiles());
  console.log(m.getActiveProfileId());
});
```

Confirm the builtin profile appears, then reload and confirm it's still
there (seed logic only runs once).

## Notes

- This task has no Web Serial dependency and no UI — it's pure storage
  logic, kept separate so [D08](08-config-tab.md) (the actual upload UI) can
  be a thin wrapper around it.
- Verified 2026-09-05: seeding, an invalid upload rejection, a valid
  upload, active-id/upload persistence across reload, and a refused
  builtin-delete were all exercised in the integrated browser against the
  served page.
