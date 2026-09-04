# Internal API Contract — RP2350B ↔ C5 ↔ S3
# Neuro-Sniffer Pro v0.3.3.0 | docs/decisions/ADR-015

## Driving principle

This project exists to run safety-first test scenarios against mission-
critical wireless devices. That isn't a feature bolted onto the RF
capability — it *is* the product. So the internal API is not "send RF
commands, with safety checks added on": every message that can result in
transmission carries its test-scenario context as a required field, not
an optional one. A command with no safety context is not a valid command.

## Architecture

    RP2350B = MASTER (owns: safety_guard, audit_logger, proximity_guard)
    C5      = RF SLAVE (owns: Wi-Fi, BLE, Zigbee, CSI, OTA client)
    S3      = AI SLAVE (owns: TFLite inference, Sub-GHz, NFC, fuzzy_engine)

RP2350B is the only chip that may authorise a transmission. C5 and S3
propose; RP2350B disposes. Neither slave chip can key a radio without a
grant from `safety_guard_request_tx()` on the master.

## Transport

SPI, RP2350B as master, GPIO5=CS→C5, GPIO6=CS→S3 (shared CLK/MOSI/MISO,
per hardware/PCBWAY_KICAD_AI_PROMPT_SPINE.md). C5↔S3 keep a direct UART
for the OTA relay path only — that path never carries TX commands.

## Message envelope (v1.0)

Every command from master to slave:

```json
{
  "v": 1,
  "seq": 1042,
  "cmd": "WIFI_DEAUTH",
  "safety_ctx": {
    "test_mode": true,
    "target_class": "medical",
    "shielded_env_confirmed": true,
    "written_permission_ref": "ENG-2026-0114"
  },
  "params": { "target": "broadcast", "channel": 6 }
}
```

`safety_ctx` is mandatory on every command that can transmit. A slave
receiving a TX-capable command with no `safety_ctx` block rejects it
locally as malformed, before it even reaches the master's safety guard —
belt and braces, not relying on one layer alone.

Response:

```json
{ "v": 1, "seq": 1042, "status": "ok|blocked|error", "block_reason": "...", "data": {} }
```

`block_reason` uses the same enum as `task_safety_guard.c`'s
`SafetyBlockReason_t` — one vocabulary for "why not" across every layer.

## Ownership table

| Command prefix | Owner | Safety-gated? |
|---|---|---|
| `WIFI_*`, `BLE_*`, `ZIGBEE_*`, `CSI_*` | C5 | TX commands: yes |
| `SUBGHZ_*`, `NFC_*`, `FUZZY_*`, `TFLITE_*` | S3 | TX/noise-gen: yes |
| `AUDIT_*`, `PROXIMITY_*`, `SAFETY_*` | RP2350B | n/a — master owns these |

## Dead-lock prevention

- All cross-chip calls are async (FreeRTOS queue in, queue out). No task
  blocks waiting on another chip's SPI response past a 2s timeout.
- A slave never calls back into the master synchronously — it posts to
  the master's inbound queue and continues its own loop.

## Performance budget

| Command class | Max latency |
|---|---|
| Safety block/allow decision | 50ms (matches task_safety_guard.c tick) |
| Passive scan/read | 500ms |
| TX authorise → RF live | 100ms |
| OTA chunk | 2s |

## Chip-swap entropy

A future chip swap (e.g. C5 → a hypothetical C7) only requires a new
implementation behind the same `cmd` vocabulary and `safety_ctx` contract.
RP2350B's safety logic and S3's AI pipeline don't change, because neither
ever assumed anything about C5's internals — they only ever spoke this
envelope.

---
*ADR-015 | v0.3.3.0 | robotard*