# D01 - App Shell and PWA Scaffold

**Status:** Done (2026-09-05)

## Goal

A static, installable app shell that serves locally with no build step: tab
navigation (Current | Timeseries | Console | Config), a Connect button, and
PWA plumbing (manifest + service worker) — before any real serial/decoding
logic exists. This lets installability (item 7 of the original request) be
verified early and independently of the harder modules.

## Scope

- `dashboard/index.html` — page shell: header with Connect button and
  connection status, tab bar, a mount `<div>` per view, `<script type="module"
  src="js/app.js">`.
- `dashboard/css/app.css` — app-wide styles (tab bar, tiles grid, layout).
  Minimal, no CSS framework.
- `dashboard/manifest.webmanifest` — name, short_name, icons, `display:
  standalone`, `start_url`/`scope` relative to `dashboard/`.
- `dashboard/icons/icon-192.png`, `icon-512.png` — simple placeholder icons
  (can be a plain generated square/glyph; not a design deliverable).
- `dashboard/sw.js` — service worker: precache the app shell's static assets
  on install, cache-first fetch handler, versioned cache name so a later
  redeploy can bust it.
- `dashboard/js/app.js` — registers the service worker, wires tab-switching
  (show/hide the four view containers), and stubs the Connect button (no
  real Web Serial call yet — that's [D03](03-serial-transport-and-elm-session.md)).

## Steps

1. Write `index.html` + `app.css` with the four tabs and Connect button;
   tab-switching logic lives in `app.js`.
2. Write `manifest.webmanifest` and link it from `index.html`'s `<head>`.
3. Generate two simple placeholder icons and reference them from the
   manifest.
4. Write `sw.js` with an install-time precache listing every static file
   that exists so far, and register it from `app.js`.
5. Confirm nothing here depends on Web Serial, localStorage content, or any
   other not-yet-built module — this task should work standalone.

## Acceptance criteria

- Serving `dashboard/` via any static file server and opening it in
  Chrome/Edge shows the tab bar and Connect button with no console errors.
- DevTools > Application > Manifest shows the manifest parsed correctly with
  both icon sizes.
- DevTools > Application > Service Workers shows the worker registered and
  activated; the install icon/prompt is available (installability checklist
  passes in DevTools > Lighthouse or the Application panel).
- Switching tabs shows/hides the right container; no tab's content needs to
  exist yet (empty placeholders are fine).

## Verification

Manual, in-browser:

```powershell
npx serve dashboard
```

Open the printed URL in Chrome, check the Application panel's Manifest and
Service Worker sections, click through all four tabs.

## Notes

- Icons are a placeholder here; swap for something real whenever it's
  convenient, it's not gating any other task.
- `sw.js`'s precache list will need updating as later tasks add files —
  call this out explicitly in each later task's steps rather than relying on
  someone remembering.
- Icons were generated as SVG (`image/svg+xml`) rather than PNG — Chrome/Edge
  accept vector manifest icons, and it avoids needing an image-generation
  tool for a placeholder glyph. Swap for PNG later if a target ever rejects
  SVG icons.
- Verified 2026-09-05: served via `python -m http.server --directory
  dashboard`, opened in the integrated browser — tab switching and the
  Connect stub both work, no console errors on load.
