# Dashboard Task Index

Execution board for the ESP-OBD dashboard SPA. Mirrors the conventions of the
firmware's [../../../docs/tasks/INDEX.md](../../../docs/tasks/INDEX.md), with
a separate `D`-prefixed numbering so it isn't confused with the firmware's
`T`-numbered tasks — this is a standalone browser app, not firmware work.

## Status key

| Mark | Meaning |
|---|---|
| `[ ]` | Planned - not started |
| `[~]` | In progress - one active implementation task at a time |
| `[!]` | Blocked - needs a decision, hardware, or external result |
| `[x]` | Done - acceptance criteria and tests are satisfied |

## Backlog

| Status | Task | Depends on | Outcome |
|---|---|---|---|
| `[x]` | [D01 - App shell and PWA scaffold](01-app-shell-and-pwa-scaffold.md) | - | Installable static shell with tab navigation, serving locally |
| `[x]` | [D02 - Config store and bundled generic profile](02-config-store-and-bundled-profile.md) | D01 | Persisted signal profiles, seeded with bundled SAEJ1979 |
| `[x]` | [D03 - Serial transport and ELM327 session](03-serial-transport-and-elm-session.md) | D01 | Web Serial connect/init/command-queue against the real adapter |
| `[x]` | [D04 - Signal decoder and synthetics](04-signal-decoder-and-synthetics.md) | - | OBDb `fmt`/`synthetics` decoding, verified against fixtures |
| `[x]` | [D05 - Poll engine](05-poll-engine.md) | D02, D03, D04 | Continuous polling loop producing decoded signal updates |
| `[x]` | [D06 - Current tab](06-current-tab.md) | D05 | Live tile grid view |
| `[x]` | [D07 - Console tab](07-console-tab.md) | D03 | Raw TX/RX log view |
| `[x]` | [D08 - Config tab](08-config-tab.md) | D02 | Upload/select/delete profile UI |
| `[x]` | [D09 - Timeseries tab](09-timeseries-tab.md) | D05 | Rolling buffers + canvas line chart view |
| `[!]` | [D10 - Integration and hardware validation](10-integration-and-hardware-validation.md) | D06, D07, D08, D09 | Wired-up app, validated against real ESP-OBD hardware |
| `[x]` | [D11 - Protocol status display](11-protocol-status-display.md) | D03, D07 | Show current CAN protocol mode in UI |
| `[x]` | [D12 - Custom PID discovery with backoff](12-custom-pid-discovery-backoff.md) | D05, D08 | Smart NO DATA handling and user visibility toggle |
| `[ ]` | [D13 - Bundle Honda and Honda-Civic profiles](13-bundle-honda-profiles.md) | D02, D05 | Manufacturer-specific OBDb profiles as built-in options |

## Working rules

1. Work tasks in dependency order unless the index is updated with a reason.
   D04 has no dependencies and can be done any time before D05.
2. Before starting a task, change its status to `[~]` and add a start date.
3. A task is `[x]` only when every acceptance criterion is met. This project
   has no Node/npm toolchain available in the dev sandbox (see
   [D04](04-signal-decoder-and-synthetics.md)'s notes) — browser-based
   verification (a static test-runner page, or manual exercise via the
   Connect flow) stands in for automated CI where a real test runner would
   normally go.
4. Record a blocker in the task file, then mark the index `[!]`; do not hide
   it behind a vague TODO.
5. Keep changes small: one task should be one focused set of new/changed
   files under `dashboard/`.

## Source documents

- [Plan](../PLAN.md) — the original request and research this board was
  scoped from.
- [Architecture](../ARCHITECTURE.md) — module boundaries and data flow each
  task implements a piece of.
- [Design decisions](../DESIGN_DECISIONS.md) — rationale for choices tasks
  should not casually re-litigate (no server, no bundler, localStorage, etc.).
