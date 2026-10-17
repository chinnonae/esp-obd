# ELM327 Implementation: Legacy ESP-OBD vs WiCAN

Scope: ELM327 command handling and the CAN request/response path only.

| Side | Source compared |
|---|---|
| **Ours (legacy)** | Commit `a1aea7f`: `src/elm/*`, `src/app/elm_application.cpp`, `src/app/elm_bluetooth_session.cpp`, `src/diagnostic/diagnostic_transport.cpp`, `src/isotp/*`, `docs/ELM_COMMAND_BEHAVIOR.md` |
| **WiCAN** | [meatpiHQ/wican-fw](https://github.com/meatpiHQ/wican-fw) at `d8922a6` (2026-09-28): `main/elm327.c`, the ELM327 emulator on the original ESP32-C3 WiCAN (the WiCAN PRO uses a dedicated OBD chip instead) |

WiCAN is GPL-3.0. This document describes its behaviour so we can compare
against it. None of its code is copied here, and none should be copied into
this project.

## Verdict

**Our implementation does more than WiCAN's.** WiCAN supports only
single-frame requests, prints multi-frame responses as raw frames, and has no
CF/CM filters, CAF0, monitor modes, extended addressing or protocol
auto-search.

**WiCAN still points at four real bugs in ours (H1–H4).** WiCAN has been tested
against real scanner apps such as Car Scanner, and several of its quirks are
workarounds for problems those apps exposed. Fix H1–H4 before calling the
reimplementation "comparable".

## Where we are ahead

| Area | Ours | WiCAN |
|---|---|---|
| Requests > 7 bytes | ISO-TP First Frame + Consecutive Frames, honours FC block size/STmin | Returns `?` |
| Multi-frame responses | Fully reassembled; validates CF sequence | Prints each raw frame, 7 bytes, no reassembly (TODO in source) |
| Protocol auto-search (`ATSP0`) | Tries 6→7→8→9, then `UNABLE TO CONNECT` | None. `SP0` makes every request fail with `BUS INIT: ...ERROR` |
| `ATCF` / `ATCM` / `ATCAF0` / `ATCFC0` / `ATCEA` / `ATCER` / `ATMA` / `ATMR` / `ATMT` / `ATCS` / `ATBD` | Implemented | Missing (`?`) |
| Empty line repeats last command | Yes (as on a real ELM327) | Ignored |
| Response-count digit (`010C1`) | Hex `1`–`F` | Decimal `1`–`9` only (FIXME in source) |
| Malformed input | `?`, no state change, no CAN TX | Non-hex characters silently parse as `0` |
| Unsupported protocols (`ATSP1`–`5`) | Rejected with `?` | Accepted with `OK`, then fail on first request |
| Line endings | Follow `ATL` everywhere | Request path hard-codes `\r` (FIXME in source) |
| Tests | Native unit tests per contract row | None for this module |

## Gaps, ranked

> **Status (2026-10-10):** fixed in commit `3cb7e30` (merged into `main`
> in `af4e67e`), each with native tests: H1–H4,
> M1–M5, L1 (manual flow control), L2 (identity now `ELM327 v1.5`) and L5
> (`ATBD`). Not changed: L3 `ATRV` stays `?` because the board has no
> voltage input, and L4 STmin/padding, because both conventions are valid.

### High: fix in the reimplementation

**H1. No RX flush before a new request.** WiCAN drains its RX queue and the
TWAI driver before every transmit. Ours never does: the TWAI filter accepts
every frame, and `DiagnosticTransport` routes any frame still queued into the
next transaction.
*Failure:* an app sends `010C1`. ECU `7E8` answers and we stop at the first
response. ECU `7E9` answers a few ms later and its frame stays queued. The next
request, `010D1`, then completes at once with `7E9`'s `41 0C …`, so the app
gets RPM data back as the speed reading.
*Fix:* drain `ICanPort::receive()` (or call `twai_clear_receive_queue`) in
`DiagnosticTransport::beginAttempt` before transmitting. Test it with the
fake port by queueing a late frame from a second ECU between two requests.

**H2. Multi-frame output with `ATH0` does not use the ELM327 format.** With
headers off, we print the reassembled payload as one long line. A real ELM327
with `CAF1`/`H0` prints the total length, then numbered lines:
```
014
0: 49 02 01 31 48 47
1: 43 4D 38 32 36 33 33
2: 41 30 30 34 33 35 32
```
Apps and libraries that keep `ATH0` (python-OBD, many Torque-style parsers)
expect this layout. WiCAN gets this wrong too, in a different way, so it is
not a reference here. Use the ELM327 datasheet section on multi-line responses
instead.
*Fix:* in `ElmApplication::formatDiagnosticResult`, emit the length line plus
`n:` lines whenever a responder's payload exceeds one frame and headers are
off.

**H3. Physical requests stop at the first complete response, which breaks
UDS "response pending" (`7F xx 78`).** Without a response-count digit, a real
ELM327 keeps listening until the timeout, restarting the timer after each
response. WiCAN behaves the same way. Ours finalizes as soon as one physical
response completes.
*Failure:* `ATSH7E0` then `22F190` (Honda-style custom PIDs use service `22`).
The ECU replies `7F 22 78` and then sends the real `62 F1 90 …` 30 ms later.
We print `7F 22 78` and show the prompt, and the real answer becomes a stale
frame (made worse by H1).
*Fix:* treat a negative response with NRC `0x78` as "not complete": keep the
receiver open and extend the deadline (P2* ≈ 5 s per ISO 14229). As a
minimum, do not count it toward `effectiveMaxResponses_`.

**H4. `ATSH` with 6 hex digits is rejected unless a 29-bit protocol is already
selected.** `parseShorthand29BitHeader` returns `?` under `SP0` (our default)
and under `SP6`/`SP8`. A real ELM327 accepts `SH xx yy zz` in any state. WiCAN
stores the header and the priority bits separately and combines them at
transmit time. The comment at `elm327.c:484-491` notes that Car Scanner sends
the header *before* changing the protocol, so on ours that header is lost.
Related problems in the same code:
- ~~Under `SP0` before the first connection, `startDiagnostic` ignores
  `customHeaderId`.~~ Not a bug: a real ELM327 also searches with the
  default (functional) headers and applies the set header afterwards.
- `ATCP` is stored but never applied; 6-digit headers are always prefixed
  with `0x18`.
- A 3-digit `ATSH` under a 29-bit protocol transmits an 11-bit frame on a
  29-bit bus. A real ELM327 builds `priority | 00 0x yz`.

*Fix:* follow WiCAN's model. Store the header bytes and the priority
separately, accept 3, 6 or 8 digits in any protocol, and build the CAN ID from
the protocol actually in use at transmit time (including after auto-search).

### Medium

**M1. Input received while a request is pending is silently dropped**
(`ElmBluetoothSession::onByte`). A real ELM327 aborts the request and prints
`STOPPED`. An app that sends a new command early, or sends a keep-alive,
loses that command and times out waiting for its reply.

**M2. Headers are forced on when more than one ECU responds** (our contract
§1.4). Neither a real ELM327 nor WiCAN does this. With `ATH0` and a `0100`
that two ECUs answer, an app expecting `41 00 …` lines receives
`7E8 06 41 00 …` instead. Remove this from the contract.

**M3. Several timeout settings are stored but have no effect.** `ATCTM1`/
`ATCTM5` and `ATAT0`–`2` never reach the transport. The collection deadline is
fixed at "end of TX + `ATST`" and is not extended when frames arrive, unlike a
real ELM327 and WiCAN, where the timer restarts after each response. Slow
multi-ECU replies get cut off. Also check `ATST00`: ours sets 4 ms, while
WiCAN treats `00` as "restore the default".

**M4. `ATZ` followed by `ATE0` in the same BLE write.** WiCAN discards the
rest of the input buffer after `ATZ` (`elm327.c:1112-1129`) because otherwise
Car Scanner falls one response out of sync. A real ELM327 also loses
characters during its reset. Ours answers both commands. Add a replay test
with the input `"ATZ\rATE0\r"` in one chunk and decide which behaviour to
match.

**M5. `ATSP` does not save the protocol.** A real ELM327 keeps the `ATSP`
protocol as its default across `ATZ`/`ATD` (`ATTP` does not save). WiCAN keeps
the current protocol across `ATZ`/`ATD` because some apps never send `ATSP`
again. Ours resets to auto-search, which costs extra search time on every
reconnect.

### Low

| # | Item | Ours | WiCAN | Note |
|---|---|---|---|---|
| L1 | `ATFCSM1`/`2`, `ATFCSH`, `ATFCSD` | Stored; manual mode only disables FC | Applied; `FCSM` validates that the header/data are set first | WiCAN's Honda Prologue profile uses `ATFCSH…;ATFCSD300000;ATFCSM1`, so real vehicle profiles depend on this |
| L2 | Identity | `ELM327 v2.2` | `ELM327 v1.3a` (`ATZ`, `AT@`), `OBDLink MX` (`ATI`) | Claiming v2.x invites apps to send v2-only commands that we reject. `v1.5` is the common clone value |
| L3 | `ATRV` | `?` (no voltage hardware) | Real voltage from an ADC | Many apps send it during init. A `?` is usually tolerated, but test it with our target apps |
| L4 | Flow Control STmin / padding | STmin `0`; padding `00` | STmin `10` ms; padding `AA` | Both valid. If an ECU drops consecutive frames, try a non-zero STmin |
| L5 | `ATBD` | Last RX frame only | n/a | Already a known gap |

## Suggested regression tests for the reimplementation

1. A late frame from a second ECU, sent between `010C1` and `010D1`, must not
   appear in the `010D1` response (H1).
2. `0902` with `ATH0` prints `014` followed by `0:`/`1:`/`2:` lines (H2).
3. `22F190` receives `7F 22 78` and then `62 F1 90 …` within P2*; the output
   contains the positive response (H3).
4. `ATSP0`, `ATSHDA10F1`, `ATSP7`, `22F190` transmits on `0x18DA10F1` (H4).
5. A byte received mid-request produces `STOPPED` and the prompt (M1).
6. With `ATH0`, a two-ECU `0100` prints two un-headered lines (M2).
7. `"ATZ\rATE0\r"` arriving in a single chunk (M4).
