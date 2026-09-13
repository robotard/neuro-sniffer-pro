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

    RP2350B = MASTER (owns: safety_guard, audit_logger, proximity_guard,
              Flipper interface, OTA instigation, tx_off verification)
    C5      = RF SLAVE (owns: Wi-Fi, BLE, Zigbee, CSI)
    S3      = AI SLAVE (owns: TFLite inference, Sub-GHz, NFC, fuzzy_engine)

RP2350B is the only chip that may authorise a transmission. C5 and S3
propose; RP2350B disposes. Neither slave chip can key a radio without a
grant from `safety_guard_request_tx()` on the master. RP2350B has no
transceiver of its own and does not acquire RF-sensing or decode duties
— see ADR-017's scope discipline. Its one RF-adjacent extension is an
independent analog TX-power verification line, not a communications role.

## Two planes (ADR-017)

**Control plane** — RP2350B↔C5, RP2350B↔S3, SPI, `safety_ctx`-gated
envelope below. Commands, authorisations, audit events. The only path
that can ever result in a transmission.

**Data plane** — C5↔S3 direct, the existing UART (previously OTA-relay
only, now also carries bulk sensor data: CSI frames, Sub-GHz IQ, model
inference results). No `safety_ctx` overhead — it isn't a command
channel. Structurally cannot transmit: its message parser has zero calls
into `safety_guard_request_tx()` or anything RF-enabling, checkable by
`grep` on the data-plane source files. Runs at lower interrupt priority
than the control plane on both chips, so sensor-data volume can never
starve a safety message.

If S3 needs C5 to do something that could transmit (e.g. widen a scan
band), that request goes out on the control plane like any other command
— authorised or refused by RP2350B — and only the result returns on the
data plane.

## Hardware abstraction — role, not chip

```
include/rf_role.h    rf_role_scan_wifi(), rf_role_get_csi_frame(),
                      rf_role_set_tx_power_mw()
include/ai_role.h    ai_role_infer(model_id, data), ai_role_load_model()
include/ctrl_role.h  ctrl_role_authorise_tx(), ctrl_role_audit_log(),
                      ctrl_role_verify_tx_off()
```

Implementation: `src/rf_role_esp32c5.c`, `src/ai_role_esp32s3.c`,
`src/ctrl_role_rp2350b.c` — named role-first, chip-second, so a future
swap adds a file rather than rewriting call sites. Public API in the
header; everything else `static` in the `.c` file; state behind an
opaque handle (`rf_role_handle_t`) so callers can't reach internals.

## Priority / QoS table (static, not adaptive — see ADR-017)

| Command class | Plane | Priority | Max latency |
|---|---|---|---|
| Safety block/allow | Control | Highest | 50ms |
| tx_off verification | Control | Highest | 50ms |
| TX authorise → RF live | Control | High | 100ms |
| Audit log write | Control | Medium | 500ms |
| CSI frame stream | Data | Medium | best-effort, 20Hz |
| Sub-GHz IQ stream | Data | Medium | best-effort |
| ai_role_infer() result | Data | Medium | model-dependent |
| Passive scan/read | Control | Low | 500ms |
| OTA chunk | Control | Low | 2s |

S3's `ai_role_infer()` is callable as a plain service by anything in the
system needing a classification — deliberately not a scheduler deciding
who runs what, just a function anyone can call. See ADR-017 for why
dynamic AI-orchestrated task placement was rejected.

## Transport

SPI, RP2350B as master, GPIO5=CS→C5, GPIO6=CS→S3 (shared CLK/MOSI/MISO,
per hardware/PCBWAY_KICAD_AI_PROMPT_SPINE.md) carries the control plane.
C5↔S3 direct UART carries the data plane (see above).

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

## Chip-swap entropy

A future chip swap (e.g. C5 → a hypothetical C7) only requires a new
implementation behind the same `cmd` vocabulary and `safety_ctx` contract.
RP2350B's safety logic and S3's AI pipeline don't change, because neither
ever assumed anything about C5's internals — they only ever spoke this
envelope.

---
*ADR-015 + ADR-017 | v0.3.3.0 | robotard*