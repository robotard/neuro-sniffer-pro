# EXTERNAL_API_SPEC.md — Neuro-Sniffer Pro v0.3.3.0

The Flipper-facing command interface — what an operator's Flipper Zero
(running `software/flipper_lua/rf_neuro.lua`) actually sends to the
RP2350B master, and what comes back. This is a different, outer layer
from `docs/INTERNAL_API_CONTRACT.md`, which documents the C5↔S3↔RP2350B
SPI control-plane protocol (JSON-enveloped, `safety_ctx`-gated) that
operators never see directly.

**This document describes what's actually implemented in
`firmware/rp2350b_master/src/main.cpp`'s `route_cmd()` and
`software/flipper_lua/rf_neuro.lua`, not an aspirational design.** The
real protocol today is a flat text command line, not a structured
envelope. Where that's a gap rather than a deliberate choice, it's
called out plainly in §6 rather than papered over.

## 1. Transport

UART0, 115200 baud, 8N1, GPIO0 (TX) / GPIO1 (RX) between Flipper Zero
and RP2350B. Each command is a single line, terminated by `\n` (a `\r`
immediately before it is silently stripped). Max line length 254 bytes
— `task_flipper()`'s receive buffer is `char buf[256]` and force-flushes
at 254 characters even without a newline, so a command (plus its
longest parameter, e.g. a BSSID or SSID string) must fit in that budget.

## 2. Command format

Plain text, space-separated: `<COMMAND> [param1] [param2] ...`

No prefix byte, no sequence number, no checksum. Examples straight from
`rf_neuro.lua`:

```
WIFI_SCAN_2G
DEAUTH AA:BB:CC:DD:EE:FF
EVIL_TWIN MyCorpWiFi
SUBGHZ_SWEEP 433 435
TX_POWER 50
SAFETY_STATUS
REBOOT
```

## 3. Command routing (as implemented in `route_cmd()`)

| Prefix / exact match | Routed to | Notes |
|---|---|---|
| `WIFI_*`, `BLE_*`, `ZIGBEE_*`, `OTA_*`, `GHOST_*`, `WARDRIVE*` | ESP32-C5 (SPI, CS=GPIO5) | |
| `SUBGHZ_*`, `NFC_*`, `AI_*`, `MODEL_*` | ESP32-S3 (SPI, CS=GPIO6) | |
| `SAFETY_STATUS` | RP2350B, handled locally | Returns JSON, see §4 |
| `REBOOT` | RP2350B, handled locally | Calls `watchdog_reboot()`, no response sent |
| `GPS_STATUS` | *(reserved — GPS is a planned future feature, not yet implemented)* | See §6 — currently falls through with no match and gets no response |
| anything else unmatched | silently dropped | `route_cmd()` has no default/unknown-command branch; a typo or unimplemented command gets no `ERROR:` reply at all today |

A command is forwarded to C5/S3 as the exact same text line over SPI —
there's no re-encoding into the internal JSON envelope at this layer.
That re-encoding (or lack of it) is itself part of the ADR-017 gap
tracked in `main.cpp`'s TODO comment and restated in §6 below.

## 4. Responses

No generic response envelope exists. Three concrete reply shapes exist
in the current firmware, all newline-terminated:

**Safety status** (`SAFETY_STATUS` → ), a single JSON line:
```
SAFETY:{"kill":0,"es_alert":0,"rf_vcc":0,"pwr_cap_mw":10,"block_reason":0}
```
Field meanings match `SafetyState_t` in `task_safety_guard.c`: `kill`
(1 = kill switch engaged, i.e. RF permitted), `es_alert` (1 = ES-band
proximity currently detected), `rf_vcc` (1 = RF power rail actually
live), `pwr_cap_mw` (enforced TX ceiling), `block_reason` (numeric
`SafetyBlockReason_t` enum value, 0 = no block).

**Error**, plain text, one case implemented today:
```
ERROR:SAFETY_LOCKED
```
Sent when a transmit-capable command (`DEAUTH`, `EVIL_TWIN`,
`BEACON_FLOOD`, `WIFI_JAM`, `HANDSHAKE_CAP`, `BLE_FLOOD`, `BLE_SPOOF`,
`BLE_FUZZ`, `SUBGHZ_REPLAY`, `SUBGHZ_JAM`, `NFC_WRITE`, `NFC_CLONE`,
`DUCKY_START`) is received while the kill switch is off or an ES alert
is active. There is currently exactly one error string in the whole
protocol — no error code enum beyond this.

**Unsolicited alert**, sent without any command having been issued:
```
ALERT:EMERGENCY_SHIELD_ACTIVE
```
Pushed once on the rising edge of ES-band detection (ADR-003
notify-not-block), from `task_flipper()`'s 1ms poll loop. The Flipper
side has to treat incoming UART lines as either a reply to its last
command or an unprompted alert — there is no sequence/request ID to
tell them apart. In practice this is manageable because the Lua app is
only ever waiting on one command at a time and the alert line is
visually distinct, but it means a strict request/response client
library isn't possible against this protocol as it stands today.

Everything routed on to C5/S3 (`WIFI_*`, `SUBGHZ_*`, etc.) currently has
**no defined response format at all** in this tree — those slave-side
UART/SPI reply handlers aren't implemented yet. The Lua menu fires the
command and doesn't wait for a structured reply.

## 5. Command reference (from `rf_neuro.lua`, grouped by menu)

| Command | Params | Menu |
|---|---|---|
| `WIFI_SCAN_2G`, `WIFI_SCAN_5G`, `WIFI_SCAN_BOTH` | — | Scan |
| `BLE_SCAN`, `ZIGBEE_SCAN`, `SUBGHZ_SCAN` | — | Scan |
| `WARDRIVE_START` | — | Scan |
| `DEAUTH` | `<BSSID>` | Attack |
| `HANDSHAKE_CAP` | `<BSSID>` | Attack |
| `EVIL_TWIN` | `<SSID>` | Attack |
| `BEACON_FLOOD` | — | Attack |
| `WIFI_JAM` | — | Attack — gated by an "authorised use only" + "SHIELDED ONLY" confirm dialog in the Lua app itself, before the command is even sent |
| `BLE_FLOOD`, `BLE_FUZZ` | — | Attack |
| `BLE_SPOOF` | `<name>` | Attack |
| `DUCKY_START` | — | Attack |
| `NFC_READ`, `NFC_WRITE`, `NFC_CLONE`, `NFC_BRUTEFORCE`, `NFC_DESFIRE_ENUM`, `NFC_SAVE`, `NFC_LOAD` | — | NFC/RFID |
| `SUBGHZ_CAPTURE`, `SUBGHZ_REPLAY`, `SUBGHZ_ROLLING`, `LORA_DECODE` | — | Sub-GHz |
| `SUBGHZ_SWEEP` | `<start MHz> <stop MHz>` | Sub-GHz |
| `SUBGHZ_JAM` | `<MHz>` | Sub-GHz — same "SHIELDED ONLY" confirm pattern as `WIFI_JAM` |
| `GHOST_START`, `GHOST_STOP`, `GHOST_STATUS`, `GHOST_EXPORT_FP` | — | Ghost + AI |
| `AI_CROWD`, `AI_FINGERPRINT`, `AI_FUZZY` | — | Ghost + AI |
| `OTA_CHECK` | — | System |
| `OTA_START_C5`, `OTA_START_S3` | — | System — "Flash? Reboots." confirm in Lua app |
| `SAFETY_STATUS` | — | System |
| `GPS_STATUS` | — | System (see §6 — not yet routed) |
| `TX_POWER` | `<10\|50\|100>` | System |
| `REBOOT` | — | System — "Reboot?" confirm in Lua app |
| `MODEL_LIST` | — | top-level menu |
| `NEURO_PING` | — | sent once on app start, no documented reply |

## 6. Known gaps (stated plainly, consistent with `docs/SAFETY_COMPLIANCE.md`)

- **No command envelope, versioning, or request/response correlation.**
  This spec describes a flat text line protocol because that's what's
  implemented. A structured envelope (sequence numbers, a `v` field,
  explicit per-command response types) does not exist at this layer —
  only the internal C5/S3 control plane has that (see
  `docs/INTERNAL_API_CONTRACT.md`).
- **No defined error code set beyond `ERROR:SAFETY_LOCKED`.** An
  unrecognised command, a malformed parameter, or a C5/S3-side failure
  currently produce no error reply at all — the Flipper side has no way
  to distinguish "command accepted, working on it" from "command not
  understood."
- **No timeout contract.** Nothing in `main.cpp` or `rf_neuro.lua`
  defines how long the Lua app should wait for a reply before treating
  a command as failed.
- **`GPS_STATUS` is in the Lua menu but not in `route_cmd()`'s routing
  table.** This is expected, not a bug: GPS is a planned future feature.
  The NEO-M9N UART (GPIO8/9) is already reserved in `main.cpp`'s pin map
  and `audit_logger.c` already carries an `extern` stub for the
  NMEA-parsing task that will own it (`task_gps_parser.c`, tracked as
  Trello [FW-005](https://trello.com/c/eZfqFQU0), currently Backlog on
  the Firmware board, not started). Sending `GPS_STATUS` today gets no
  response at all — not even an error — which is fine for now but worth
  remembering once FW-005 lands, so this routing table and the error
  path both get updated in that same change rather than drifting.
- **No response format defined for anything forwarded to C5/S3.** Scan
  results, capture data, AI inference output, etc. all route out over
  SPI but nothing in this tree defines how (or whether) a reply comes
  back to the Flipper over UART. This is the single biggest gap between
  "what the menu lets you request" and "what the protocol can actually
  deliver back," and is a reasonable next milestone once C5/S3-side
  firmware for these commands exists.
- **`route_cmd()`'s safety pre-filter is non-authoritative** — see the
  TODO at the top of `main.cpp` and ADR-017. The real TX gate is
  `safety_guard_request_tx()` inside `task_safety_guard.c`; this
  UART-facing layer only does a cheap early reject so the operator gets
  fast feedback, not the actual safety decision.

## References
- `firmware/rp2350b_master/src/main.cpp` (`route_cmd()`, `task_flipper()`)
- `software/flipper_lua/rf_neuro.lua`
- `docs/INTERNAL_API_CONTRACT.md` (the internal SPI control-plane protocol this sits above)
- `docs/SAFETY_COMPLIANCE.md`
- `firmware/rp2350b_master/src/task_safety_guard.c`
