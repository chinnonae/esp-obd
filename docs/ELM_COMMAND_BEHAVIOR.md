# ELM Command Behaviour Contract

This document is the implementation and unit-test contract for the ESP-OBD
adapter. It intentionally specifies a **CAN-only** ELM327-compatible adapter,
not an emulation of hardware that is not present on the board.

The reference ELM327 datasheet describes a wider device with J1850, ISO/KWP,
voltage-measurement, and ignition-monitor pins. Those capabilities are outside
the ESP32 + Classical-CAN-transceiver hardware scope. Unsupported commands
must return `?`; they must not return invented values or a misleading `OK`.

## 1. Test conventions

### 1.1 Command input

- Commands are case-insensitive.
- ASCII spaces in ELM commands are ignored. For example, `AT SH 7E0` and
  `atsh7e0` are equivalent.
- A command completes on carriage return. A following linefeed is ignored.
- An empty command repeats the most recently non-empty command. An empty
  command before any previous command produces no response. `ATZ`/`ATWS`
  forget the previous command, so empty commands right after a reset produce
  no response (Car Scanner sends several while waiting for the banner).
- A malformed, unknown, or unsupported command returns `?` followed by the
  normal response ending. It must not modify session state or transmit CAN.
- Any valid non-empty command becomes the command that an empty command will
  repeat, including a command that later produces `NO DATA`.

### 1.2 Response ending and prompt

The command handler emits a response ending; the Bluetooth transport appends
the prompt. Test the handler and transport separately.

| Setting | Handler response ending | Bluetooth result after response |
|---|---|---|
| `ATL0` (default) | `\r\r` | `\r\r\r>` |
| `ATL1` | `\r\n\r\n` | `\r\n\r\n\r\n>` |

- The transport echoes received characters only while echo is enabled.
- `ATMA`, `ATMR`, and `ATMT` enter monitor mode and do **not** immediately
  produce a response ending or prompt.
- During monitor mode, each received matching CAN frame is formatted as one
  line. Any received character stops monitoring, discards that character, and
  returns `STOPPED` with a normal ending and prompt.
- The same applies to a diagnostic request still in progress: any received
  character abandons it and produces `STOPPED`, ending and prompt. That
  character and the rest of its line, up to and including the CR, are
  discarded and never executed (the reference adapter answers `ATRV` sent
  mid-request with only `STOPPED`). If the reply had already arrived, the
  reply is sent first and the character is handled as normal input.
- The first request after `ATSP0` (or after a disconnect) prints
  `SEARCHING...` and a line break before its reply, which includes every
  responding ECU.
- After `ATZ`/`ATWS`, input that had already been received when the reset ran
  (e.g. the `ATE0` in a single `ATZ\rATE0\r` write) is discarded, as a real
  ELM327 loses it while restarting.
- Bluetooth is the only ELM command transport. UART0 is debug-only. No line
  beginning with `#DBG:` may be sent to Bluetooth.

### 1.3 Defaults after `ATZ` or `ATD`

| State | Default |
|---|---|
| Identity | Value of `ELM_VERSION` (`ELM327 v1.5`: what the reference adapter in the Car Scanner field log reports; a v2.x claim invites v2-only commands this adapter rejects) |
| Echo | on |
| Linefeeds | off |
| Spaces | on |
| Headers | off |
| Display DLC | off |
| Automatic CAN formatting | on (`CAF1`) |
| Automatic flow control | on (`CFC1`) |
| Allow long messages | off (`NL`) |
| Responses | on (`R1`) |
| Adaptive timing | mode 1 |
| Protocol | the protocol last saved by `ATSP` (initially automatic CAN-only search, `SP0`) |
| Request CAN ID before a protocol is found | `7DF` |
| Receive address, mask, filter | cleared |
| Response timeout | `0x32 * 4 ms` (200 ms nominal) |
| Custom header / CAN extended address | disabled |
| Monitor mode / protocol connection | inactive / disconnected |

`ATZ` performs the reset and returns the identity string. `ATD` performs the
same configuration reset but returns `OK`. `ATWS` performs the reset, returns
the identity string, and preserves neither protocol nor custom header.

Tests should assert the configured `ELM_VERSION` constant rather than spell
out the identity string.

### 1.4 Formatting rules

- Bytes are uppercase two-digit hexadecimal.
- `ATS1` separates adjacent printed bytes with one ASCII space; `ATS0` does
  not. There is never a trailing space.
- `ATH0` + `CAF1` prints only the ISO-TP payload. A single-frame response
  `06 41 00 BE 1F B8 10 00` prints `41 00 BE 1F B8 10`.
- `ATH0` + `CAF1` prints a multi-frame payload the way an ELM327 does: the
  payload length as three hex digits on its own line, then one line per
  frame prefixed with its index `0:`..`F:` (wrapping). A 20-byte VIN reply
  prints `014`, `0: 49 02 01 31 48 47`, `1: 43 4D 38 32 36 33 33`,
  `2: 41 30 30 34 33 35 32`. The final frame's padding bytes are not printed.
- Lines within one response end with a single `\r` (`\r\n` with `ATL1`); the
  last line is followed by the response ending, e.g.
  `7E8 02 41 00\r7E9 02 41 00\r\r`.
- `ATH1` prints CAN ID, optional DLC, then raw frame bytes. With `ATD0`, the
  same frame prints `7E8 06 41 00 BE 1F B8 10`; with `ATD1`, it prints
  `7E8 8 06 41 00 BE 1F B8 10 00`. `ATD0` trims only Single Frames: First
  and Consecutive Frames are always printed in full, padding included, as the
  reference adapter does (`18DAF110 21 00 00 00 55 55 55 55`).
- `CAF0` prints and transmits caller-supplied raw CAN data. It does not add or
  remove ISO-TP PCI bytes.
- Several responders under `ATH0` print as plain un-headered lines, one per
  responder, as on an ELM327. A client that must tell ECUs apart uses `ATH1`.
- A silent bus produces `NO DATA`. A CAN transmit or driver failure produces
  `CAN ERROR`. A protocol auto-search that exhausts all four CAN protocols
  produces `UNABLE TO CONNECT`.

## 2. Command contract

### 2.1 Core session commands

| Syntax | Required behaviour | Unit-test assertions |
|---|---|---|
| `ATZ` | Restore defaults, wait for reset delay on target, return identity. | Defaults are exact; no CAN frame; response is `ELM_VERSION` + ending. |
| `ATWS` | Warm-start: restore defaults and return identity. | Same observable state as `ATZ`; no retained header. |
| `ATD` | Restore defaults without reset banner. | `OK`; defaults exact; no CAN. |
| `ATI` | Return the configured identity string. | Exact `ELM_VERSION` + ending. |
| `AT@1` | Return fixed adapter description: `ESP-OBD CAN Adapter`. | Exact text + ending. |
| `AT@2` | Return the 12-hex-digit device identifier. | Default is `FFFFFFFFFFFF`; no CAN. |
| `AT@3hhhhhhhhhhhh` | Validate exactly 12 hexadecimal digits and store the device identifier in NVS. | Valid: `OK`, retained after reset/power cycle. Invalid: `?`, previous ID unchanged. |
| `ATE0` / `ATE1` | Disable / enable character echo. | `OK`; transport echo behavior changes only after response. |
| `ATL0` / `ATL1` | Disable / enable linefeeds. | `OK` uses the *new* line-ending mode. |
| `ATS0` / `ATS1` | Disable / enable spaces. | `OK`; a following frame uses selected separator. |
| `ATH0` / `ATH1` | Disable / enable CAN headers. | `OK`; a following frame hides/shows ID and PCI. |
| `ATD0` / `ATD1` | Disable / enable displayed CAN DLC. | `OK`; affects only headers-on output. |
| `ATR0` / `ATR1` | Suppress / enable received responses. | `R0` transmits a valid request but prints no ECU data and returns the normal ending. |
| `ATAL` / `ATNL` | Allow / disallow long messages. Stored only (CAN frames never exceed 8 bytes); Car Scanner sends `ATAL` during init. | `OK`. |
| `ATM0` / `ATM1` | Disable / enable persistence of supported settings. | `OK`; `M0` prevents later setting changes from being written to NVS. |
| `ATFE` | Forget persisted supported settings and restore defaults. | `OK`; a reconstructed session has factory defaults. |
| `ATRD` | Read the saved data byte. | Returns two uppercase hex digits. |
| `ATSDhh` | Store one byte in NVS. | Valid: `OK`, `ATRD` returns `hh`; invalid argument preserves old value. |

`AT@1`, persistence, and `ATFE` are adapter-defined compatibility features;
they do not assert that this ESP32 has the original ELM327 EEPROM layout.

### 2.2 Protocol and timeout commands

| Syntax | Required behaviour | Unit-test assertions |
|---|---|---|
| `ATSP0` | Select automatic CAN-only search and save it as the default. | `OK`; disconnected; next request tries `6`, `7`, `8`, `9` in that order. |
| `ATSP6` | Select ISO 15765-4, 11-bit, 500 kbit/s. | `OK`; TWAI is 500 kbit/s; default request ID is `7DF`. |
| `ATSP7` | Select ISO 15765-4, 29-bit, 500 kbit/s. | `OK`; TWAI is 500 kbit/s; default request ID is `18DB33F1`. |
| `ATSP8` | Select ISO 15765-4, 11-bit, 250 kbit/s. | `OK`; TWAI is 250 kbit/s; default request ID is `7DF`. |
| `ATSP9` | Select ISO 15765-4, 29-bit, 250 kbit/s. | `OK`; TWAI is 250 kbit/s; default request ID is `18DB33F1`. |
| `ATTPx`, `ATTPAx` | Try one supported protocol, optionally falling back to auto search. | Same selection semantics as `SP`, but does not change the saved default; no CAN until a data request. |
| `ATDP` | Describe selected/connected protocol. | Exact CAN protocol description; prefix `AUTO, ` only after automatic search connects. |
| `ATDPN` | Return selected protocol number. | `0` while searching; `A6`..`A9` after auto connection; otherwise `6`..`9`. |
| `ATSThh` | Set response timeout to `hh * 4` milliseconds; `ATST00` restores the default (200 ms). | `OK`; valid range `00`..`FF`; malformed input changes nothing. |
| `ATAT0`, `ATAT1`, `ATAT2` | Set adaptive timing mode (see below). | `OK`; state is 0, 1, or 2. The wait never exceeds `ATST` x `ATCTM`. |
| `ATCTM1`, `ATCTM5` | Set the CAN timeout multiplier to 1 or 5. | `OK`; the response timeout is `ATST` x the multiplier. |

`ATSP6`-`9` and `ATSP0` also save the protocol as the default that `ATZ`,
`ATD` and `ATWS` return to (in RAM only; it is not written to NVS).

Response timing: the first response is awaited for the full timeout
(`ATST` x `ATCTM`), and the timer restarts after every accepted frame, as on
an ELM327. With `ATAT1`/`ATAT2`, once some earlier request has received
responses, the wait after a response is shortened to about how long the
slowest recent responder took after transmission (`AT1`: 2x, at least 20 ms
after the latest frame; `AT2`: 1.5x, at least 8 ms). An ECU that has never
answered within that window can therefore be missed in adaptive mode; use
`ATAT0` or a response-count digit if that matters. Collection never ends
while a multi-frame response is still arriving.
| `ATPC` | Close current protocol and leave monitor mode. | `OK`; protocol remains selected but disconnected; no driver uninstall required. |

`ATSP1` through `ATSP5`, `ATSP A` through `ATSP C`, and their `TP` forms
return `?`. They must not be accepted and later fail on the first request.

### 2.3 CAN addressing, filtering, and flow control

| Syntax | Required behaviour | Unit-test assertions |
|---|---|---|
| `ATSHhhh` | Set header bytes `00 0h hh`. | `OK`; reject values above `7FF`; next 11-bit TX uses `hhh`; next 29-bit TX uses priority + `000hhh`. |
| `ATSHhhhhhh` | Set header bytes `hh hh hh`. Accepted in any protocol state (scanner apps send it before `ATSP7`). | `OK` under `ATSP0`/`6`-`9`; 29-bit TX is priority + `hhhhhh` (`0x18hhhhhh` by default); 11-bit TX uses the low 11 bits. |
| `ATSHhhhhhhhh` | Set the priority bits (first byte, low 5 bits) and header bytes (last 3). | `OK`; reject values above `1FFFFFFF`; 29-bit TX is exactly `hhhhhhhh`; 11-bit TX uses the low 11 bits. |
| `ATCPhh` | Set the five priority bits used with a 29-bit header. | `OK`; changing priority changes only the applicable ID bits of the next 29-bit TX. |

The CAN ID is built from the stored header at transmit time, from the
protocol actually in use, so a header set before `ATSP`, or before an
auto-search connects, still applies afterwards. During an auto-search
itself, requests go to the functional ID (as on an ELM327).
| `ATCRA` / `ATAR` | Clear explicit receive address and CAN mask/filter. | `OK`; default OBD response range applies. |
| `ATCRAxxx` / `ATCRAxxxxxxxx` | Set one exact receive CAN ID. | `OK`; non-matching frames are ignored. |
| `ATCFxxx` / `ATCFxxxxxxxx` | Set CAN ID filter. | `OK`; pairs with current mask. |
| `ATCMxxx` / `ATCMxxxxxxxx` | Set CAN ID mask. | `OK`; a frame is accepted iff `(id & mask) == (filter & mask)`. |
| `ATCAF0` / `ATCAF1` | Disable / enable automatic ISO-TP formatting. | `OK`; `CAF1` creates/removes PCI; `CAF0` preserves raw data. |
| `ATCFC0` / `ATCFC1` | Disable / enable automatic ISO-TP flow control. | `OK`; a received First Frame creates no FC with CFC0 and one FC with CFC1. |
| `ATFCSM0` / `ATFCSM1` / `ATFCSM2` | Set flow-control mode: automatic / user header+data / user data with derived header. `FCSM1` requires `ATFCSH` and `ATFCSD` to be set first, `FCSM2` requires `ATFCSD`. | `OK`, or `?` with no change if the required values are missing; the next First Frame's Flow Control uses the `ATFCSH` id (`FCSM1`) and exactly the `ATFCSD` bytes, zero padded (`FCSM1`/`FCSM2`). `ATCFC0` still suppresses Flow Control. |
| `ATFCSHhhh` / `ATFCSHhhhhhhhh` | Set the CAN ID for manual flow-control frames. | `OK`; FC uses this ID in manual-header mode. |
| `ATFCSD[1..5 bytes]` | Set manual FC payload bytes. | `OK`; remainder is zero padded; FC output begins with supplied bytes. |
| `ATCEA` | Disable CAN extended addressing. | `OK`; payload begins with PCI. |
| `ATCEAhh` | Enable extended addressing with transmit address `hh`. | `OK`; inserted before PCI on each outgoing frame. |
| `ATCERhh` | Set the required extended-address byte in received frames. | `OK`; mismatched first byte is ignored. |

All CAN filtering must be applied in software for transaction responses and
monitoring. It may additionally be compiled into a TWAI acceptance filter, but
that optimization must not change the observable matching rules.

### 2.4 Monitoring and CAN diagnostics

| Syntax | Required behaviour | Unit-test assertions |
|---|---|---|
| `ATMA` | Monitor all received CAN frames. | Enters monitor mode without immediate text; a frame prints once; input stops it. |
| `ATMRhh` | Monitor frames addressed to `hh`. | Enters monitor mode; only matching configured IDs are printed. |
| `ATMT hh` | Monitor frames sent from `hh`. | Enters monitor mode; only matching configured IDs are printed. |
| `ATCS` | Return CAN TX/RX error counters and detected bitrate/frequency. | Output fields are stable and uppercase; no CAN TX. |
| `ATCSM0` / `ATCSM1` | Disable / enable silent monitoring. | `CSM1` puts TWAI into listen-only mode before monitor; no ACK/TX. `CSM0` restores normal mode. |
| `ATRTR` | Send a remote-transmission-request frame using current header. | Exactly one RTR frame, DLC zero unless a future DLC setting is defined. |
| `ATV0` / `ATV1` | Disable / enable variable-DLC compatibility behavior. | `OK`; `V0` emits/accepts DLC 8 only; `V1` preserves actual DLC. |
| `ATBD` | Dump the latest transaction buffer. | Returns last TX and accepted RX raw CAN frames, or `NO DATA` if empty. |

### 2.5 Raw OBD and diagnostic requests

Any even-length hexadecimal command that does not start with `AT` is a CAN
diagnostic payload. The adapter does not whitelist OBD service modes. This is
a deliberate decision, not an oversight: see the write/control request policy
in [T00's decision log](tasks/00-project-baseline.md#decision-log). There is
no debug-only unlock gate for mutating services (UDS write/routine/reset
requests, etc.) — a payload transmits exactly as it would on a real ELM327.

| Case | Required behaviour | Unit-test assertions |
|---|---|---|
| Payload length 1..7, `CAF1` | Send ISO-TP Single Frame with PCI equal to payload length and zero padding. | `010C` on `7DF` transmits `02 01 0C 00 00 00 00 00`. |
| Payload length 8..4095, `CAF1` | Send First Frame, wait for valid Flow Control, then send Consecutive Frames with sequence numbers 1..15 wrapping to 0. | Frame IDs, PCI, sequence, block size, STmin, timeout and padding are exact. |
| `CAF0` | Transmit caller supplied raw CAN data, up to eight bytes. | Input does not gain PCI or altered payload bytes. |
| Functional request | Collect responses from up to eight ECUs until timeout or requested response count. | Each responder prints on its own line; with `ATH1` each line carries its source ID. |
| Physical request | Stop after its first complete accepted response unless an explicit response count is requested. | A second ECU response is ignored. |
| "Response pending" (`7F xx 78`) | Not a complete response and not printed: keep that ECU's receiver open for its real answer and extend the deadline to at least 5000 ms (ISO 14229 P2*) from the pending reply. | `22F190` answered by `7F 22 78` then `62 F1 90 ..` 300 ms later prints only the positive response; with no follow-up, `NO DATA` after P2*. |
| Stale frames | Frames already waiting in the receive queue when a request starts are discarded before it is transmitted. | A late reply from a second ECU to `010C1` is not returned for the following `010D1`. |
| Single-frame response | Decode PCI and print payload in `CAF1`/`ATH0`. | Padding is not printed. |
| Multi-frame response | Reassemble to declared length, validate CF sequence, and send an FC according to CFC/FCS settings. | No partial payload is printed as a complete response. |
| `ATH1` response | Print the actual raw received CAN frame(s), including PCI. | A First Frame and each CF are separate lines. |
| Odd length ending in `1`..`F` | Treat final nibble as maximum desired responses. | `01002` requests mode/PID `0100` and stops after two complete responders. |
| Odd length ending in `0`, invalid hex, no payload, or oversized `CAF0` | Reject. | `?`; no CAN frame. |

Standard OBD requests such as `0100`, `03`, `0902`, and UDS requests such as
`22F190` all follow this same transport contract. A vehicle may reject or not
implement any given service; that is not an adapter failure.

## 3. Explicitly unsupported commands

Each command below returns `?`, has no state change, and sends no CAN frame.
That exact behavior is a required parameterized test.

| Family | Commands / reason |
|---|---|
| Non-CAN vehicle protocols | `ATIFR*`, `ATFI`, `ATIB*`, `ATII`, `ATKW*`, `ATSI`, `ATSW*`, `ATWM*`; needs J1850, ISO 9141, or KWP electrical interfaces absent from the board. |
| J1939 | `ATJE`, `ATJHF*`, `ATJS`, `ATJTM*`, `ATDM1`, `ATMP*`; deliberately deferred from this OBD-II CAN implementation. |
| Voltage / ignition | `ATRV`, `ATCV*`, `ATIGN`; no battery-voltage divider or ignition-sense input. |
| Serial-rate / power-control hardware | `ATBRD*`, `ATBRT*`, `ATLP`; Bluetooth has no ELM-style baud negotiation and the board has no separate ELM power-control wiring. |
| Original ELM programmable parameters | `ATPP*`, `ATPP S`; ESP NVS settings are not an ELM PP map. |
| Other original-chip-only controls | `ATAM*`, `ATBI`, and any command not named in sections 2.1-2.4. |

## 4. Mandatory regression-test matrix

Implement a unit test for every table row above plus these cross-cutting cases:

1. Run `ATZ`, then assert every default in one test; do not rely on test order.
2. Verify each malformed command leaves a byte-for-byte copy of session state
   unchanged and leaves the fake TWAI TX queue empty.
3. Run the same formatting test with all four `ATH`/`ATS` combinations and both
   `ATD` values.
4. Test both standard and extended IDs for header, filter, receive address,
   single-frame TX/RX, and multi-frame RX/TX.
5. Test ISO-TP timeouts at: waiting for FC, between CFs, and after a partial
   response. The output must be `NO DATA` or a documented transport error, not
   a truncated success payload.
6. Test flow-control block size, STmin, sequence wrap, invalid sequence, and
   manual-flow-control header/data.
7. Test a monitor stop character is never interpreted as the next ELM command.
8. Test no UART debug text reaches a Bluetooth capture, even with debug level 3.
9. Replay initialization traces from target scanner apps against the native
   fake-TWAI suite before testing a vehicle.

## 5. Implementation status labels

When adding a test, use one of these labels in its name or comment:

- **Contract**: behavior specified here and already implemented.
- **Target**: behavior specified here but not implemented yet; add a failing
  test before implementation.
- **Unsupported**: must continue to return `?` until hardware or scope changes.

As of the T00-T09/T11 reimplementation (see [docs/tasks/INDEX.md](tasks/INDEX.md)),
every command family in sections 2.1-2.4 is implemented and covered by native
tests, including multi-frame request transmission and reception, and `ATRV`/
`ATIGN`/non-CAN protocol selections correctly return `?` rather than being
faked. A handful of narrower gaps remain, tracked in the relevant task files'
own Notes rather than duplicated here:

- `ATV0`/`ATV1` is stored but doesn't change wire format: frames are always
  padded to 8 bytes (T04/T05, T09).
- `ATRV` still returns `?`: the board has no confirmed battery-voltage input.
  Car Scanner polls it repeatedly (see the 2026-09-05 field log); a real
  reading needs a voltage divider on OBD pin 16 to an ADC pin first.
- `ATSH` accepts the 6-hex-digit 29-bit shorthand (`ATSHhhhhhh` ->
  `0x18hhhhhh`) that scanner apps use to retarget UDS requests at a
  specific ECU, in addition to the 3-digit and 8-digit forms, as of T12
  (found via field-log comparison against a genuine adapter on
  2026-08-29 -- see [T12](tasks/12-atsh-6digit-header.md)). Since the
  WiCAN comparison (2026-10-10) it is accepted in any protocol state, and
  the header is stored ELM-style (3 bytes + `ATCP` priority) so `ATCP` now
  applies.
- Checked against Car Scanner's log of the reference OBDII adapter
  (2026-10-10): `ATAL`/`ATNL` accepted; an interrupting line is discarded
  whole; a reset forgets the repeat command; auto-search prints
  `SEARCHING...` and returns every ECU; `ATH1` shows multi-frame padding.
  A replay of Car Scanner's init sequence is a native test.
- The TWAI port now confirms each transmitted frame (`TX_SUCCESS`). A frame
  nobody acknowledges (car off) is aborted by restarting the controller and
  the request ends in `CAN ERROR`; a bus-off controller is recovered and
  restarted before the next send. Old firmware instead went from `NO DATA`
  to `CAN ERROR` on every request until reboot (Car Scanner log,
  2026-09-05). Hardware-only code: compiled, not covered by native tests.
- Also since the WiCAN comparison: input during a running request produces
  `STOPPED`; input buffered with `ATZ` is discarded; `ATSP` sets the
  `ATZ`/`ATD` default; `ATCTM` and adaptive timing take effect; `ATST00`
  restores the default; several responders no longer force headers on.
- Since the WiCAN comparison (2026-10-10): `ATH0` multi-frame responses use
  the ELM327 length-line + `0:`/`1:` layout, response-pending (`7F xx 78`)
  replies extend the wait instead of ending it, and stale RX frames are
  discarded before each request.
- T07's ESP32 platform adapters (TWAI, NVS) and T08's Bluetooth/UART
  transports were exercised against a real board, a scanner app, and a
  live vehicle on 2026-08-28 (multi-ECU `0100`, a complete multi-frame VIN
  read, Bluetooth pairing) -- see [T07](tasks/07-esp32-platform-adapters.md)'s
  Hardware validation update. The formal bench checklist (explicit
  250 kbit/s switch, listen-only mode) and the full
  [T10](tasks/10-validation-and-release-gate.md) release gate are still
  not done.
