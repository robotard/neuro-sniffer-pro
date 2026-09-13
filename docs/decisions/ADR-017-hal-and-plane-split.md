# ADR-017 — Hardware Abstraction Layer, Control/Data Plane Split

- Status: Accepted
- Date: 2026-08-22
- Domains affected: FW-0.3.3, SW-0.2.0, SEC-0.4.0
- Tags: [architecture, hal, security, performance, inter-chip]

## Context

The internal API contract (ADR-015) established that RP2350B is master
and C5/S3 are slaves, communicating through a `safety_ctx`-gated message
envelope. That's correct for *commands* — anything that could result in
transmission must pass through RP2350B's safety guardian, no exceptions.

But it doesn't yet address two related questions: (1) should firmware
name things after chip identity (`c5_scan()`) or functional role
(`rf_scan()`), so a future chip swap doesn't ripple through every call
site; and (2) where should high-throughput sensor data — CSI frames at
20Hz×52 subcarriers, Sub-GHz IQ capture — actually flow, given
RP2350B's control-plane channel is deliberately narrow and safety-gated.

Routing bulk sensor data through RP2350B just because it's the
network's centre would make it a throughput bottleneck and put a large
volume of untrusted, complex-to-parse RF capture data directly in the
memory space of the one chip whose job is being simple enough to audit.

## Decision

**Two planes, not one.**

**Control plane** (unchanged from ADR-015): RP2350B ↔ C5, RP2350B ↔ S3,
via the `safety_ctx`-gated envelope. Commands, authorisations, audit
events. The only path that can result in a transmission.

**Data plane** (new): direct C5 ↔ S3 channel, bulk sensor data only —
CSI frames, Sub-GHz IQ samples, inference results. No safety_ctx
overhead. No code path on this channel can key a radio — wired that way
in firmware, not just documented. Reuses the existing C5↔S3 UART
(previously OTA-relay only).

**Hardware abstraction layer, named by role not chip.**

```
include/rf_role.h    — rf_role_init(), rf_role_scan_wifi(),
                        rf_role_get_csi_frame(), rf_role_set_tx_power_mw()
include/ai_role.h    — ai_role_infer(model_id, data), ai_role_load_model()
include/ctrl_role.h  — ctrl_role_authorise_tx(), ctrl_role_audit_log()
```

Implementation files named role-first, chip-second:
`src/rf_role_esp32c5.c`, `src/ai_role_esp32s3.c`, `src/ctrl_role_rp2350b.c`
— a future swap adds a file rather than rewriting call sites.

Public/private in C: public functions in the role header; everything
else `static` in the `.c` file. State behind an opaque handle
(`rf_role_handle_t`) so callers can't reach internals.

## On "letting the AI chip help orchestrate"

Explicitly not doing dynamic, AI-driven task placement between chips.
A safety-first testing tool should prefer a boring, static, auditable
priority table over an adaptive scheduler whose behaviour can't be fully
predicted ahead of time. S3's inference engine remains callable as a
plain service (`ai_role_infer()`) — that's the sensible "AI chip lends
a hand" pattern, not S3 deciding schedules.

## Security constraint

- Data-plane message parser (both ends): zero calls into anything
  RF-enabling. Checkable by `grep` for `safety_guard_request_tx|RF_VCC`
  in data-plane source — should return nothing, ever.
- Data plane runs at lower interrupt priority than control plane on
  both chips — sensor volume can never starve a safety message.
- Anything the data plane needs that could transmit goes out on the
  control plane as a normal gated command; only the result returns on
  the data plane. Data plane never self-authorises.

## Scope discipline — RP2350B is not a fourth radio

RP2350B's role stays exactly three things: Flipper interface, OTA
instigator, safety guardian. No transceiver, no RF-sensing or decode
duties — that's C5 and S3's job.

The one exception: **independent analog TX-power verification.**
`task_safety_guard.c` cuts RF_VCC and assumes power drops — true, but
unverified in the audit trail beyond "we commanded it off." A single RF
power-detector diode on the shared RF_VCC-gated rail, feeding an
RP2350B ADC input, gives the safety guardian a direct, independent
reading of actual radiated power — not something C5/S3 reported about
themselves, a hardware truth-check on them.

Why RP2350B specifically: this only earns its keep because of
RP2350B's improved ADC noise floor over RP2040 — reading a small
detector-diode voltage cleanly matters here. If RP2040's ADC would do
the job equally well, this wouldn't be a reason to prefer RP2350B.

What this is not: no protocol decode, no frequency awareness, no
scanning. One analog line, one ADC read, one audit-log field
(`tx_power_verified_mw`). `ctrl_role.h` gains exactly one function:
`ctrl_role_verify_tx_off() -> bool`. Nothing else RF-shaped is added.

## Alternatives Considered

### Single plane, everything through RP2350B (status quo)
RP2350B becomes a throughput ceiling and unnecessarily handles bulk
untrusted RF-derived data. Rejected.

### Chip-named functions (`c5_wifi_scan()`, `s3_run_model()`)
Works until a chip swap, at which point every call site changes.
Rejected in favour of role naming — costs nothing today, saves a
rewrite later.

### Dynamic AI-orchestrated task placement
Rejected for determinism/auditability reasons specific to a
safety-critical embedded system.

## Consequences

### Positive
- CSI/IQ throughput no longer bottlenecked by or exposed to RP2350B
- Chip swap touches one `_esp32c5.c` file, not every call site
- Data plane's inability to transmit is a checkable, static property

### Negative / Trade-offs
- Two channels to reason about instead of one
- Existing stub files (`ghost_tracker.c`, `task_fuzzy_engine.c` etc.)
  will be renamed/re-homed under the role convention — tracked in Trello

## References
- ADR-015 (internal API contract, control plane unchanged)
- docs/INTERNAL_API_CONTRACT.md (extended with plane split + priority table)