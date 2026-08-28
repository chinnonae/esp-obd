# ESP-OBD Task Index

This is the execution board for the clean-start reimplementation. Update the
status in this file and the matching task file whenever work starts, is blocked,
or completes.

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
| `[x]` | [T00 - Project baseline and decisions](00-project-baseline.md) | - | Stable scope, terminology, and safety policy |
| `[x]` | [T01 - Native test foundation](01-native-test-foundation.md) | T00 | Fast desktop test command and reusable fakes |
| `[x]` | [T02 - Portable CAN core](02-portable-can-core.md) | T01 | Hardware-free CAN types, filters, clock, and port interface |
| `[x]` | [T03 - ELM core and reply formatting](03-elm-core-and-formatting.md) | T01, T02 | Parser, session state, and exact ELM text without streams |
| `[x]` | [T04 - ISO-TP receive state machine](04-isotp-receive.md) | T02 | Tested single- and multi-frame reception |
| `[x]` | [T05 - ISO-TP transmit state machine](05-isotp-transmit.md) | T02 | Tested single- and multi-frame requests |
| `[x]` | [T06 - Diagnostic transactions](06-diagnostic-transactions.md) | T03, T04, T05 | Requests, responders, auto-search, and typed results |
| `[!]` | [T07 - ESP32 platform adapters](07-esp32-platform-adapters.md) | T02, T06, T11 | TWAI, clock, settings, and build integration |
| `[ ]` | [T08 - Bluetooth ELM and UART debug](08-transports-and-debug.md) | T03, T06, T07 | Clean app transport and isolated `#DBG:` console |
| `[ ]` | [T09 - CAN command families and monitoring](09-can-commands-and-monitoring.md) | T06, T07, T08 | Remaining in-scope ELM CAN commands |
| `[ ]` | [T10 - Compatibility validation and release gate](10-validation-and-release-gate.md) | T09 | Scanner-app, simulator, and vehicle evidence |
| `[x]` | [T11 - Settings persistence commands](11-settings-persistence-commands.md) | T01, T03 | `AT@2`/`AT@3`/`ATM`/`ATFE`/`ATRD`/`ATSD` handlers and settings-store interface |

## Working rules

1. Work tasks in dependency order unless the index is updated with a reason.
2. Before coding a task, change its status to `[~]`, add the start date, and
   link the failing test(s) that define the first slice.
3. A task is `[x]` only when every acceptance criterion is met and relevant
   native and target builds pass.
4. Record a blocker in the task file, then mark the index `[!]`; do not hide it
   behind a vague TODO.
5. Keep changes small: one task may produce several commits, but a commit must
   not mix unrelated tasks.

## Source documents

- [Architecture](../ARCHITECTURE.md) defines the component boundaries.
- [ELM Command Behaviour Contract](../ELM_COMMAND_BEHAVIOR.md) defines the
  externally observable command behavior.
- The legacy implementation is preserved in commit `f5c36ed` for reference,
  not as a template to copy wholesale.
