# SAFETY_COMPLIANCE.md — Neuro-Sniffer Pro v0.3.3.0

A regulatory/safety-posture summary for professional buyers and their legal
teams. This is separate from `docs/LEGAL_DISCLAIMER.md` (which covers lawful
use) — this document covers the engineering safeguards themselves, what they
do, and what they explicitly do not claim.

## 1. What this device does NOT claim

- This is not a certified product. No FCC/CE/RED compliance testing has been
  performed — there is no alpha board yet (pre-alpha, per the project brief).
  Any future production run will require its own RF emissions and safety
  compliance testing before sale in any given jurisdiction.
- The engineering safeguards below reduce the risk of *accidental* or
  *unintended* interference. They do not make unauthorised active testing
  lawful — see `docs/LEGAL_DISCLAIMER.md`.

## 2. Engineering safeguards (what's actually implemented)

| Safeguard | Mechanism | Status |
|---|---|---|
| Hardware kill switch | Physical cut of RF VCC gate (GPIO16 via SW1) — fail-safe: disengaged or disconnected = RF off | Implemented, `task_safety_guard.c` |
| Frequency whitelist | Hard reject of any TX request outside 300MHz–6GHz | Implemented, `task_safety_guard.c` |
| TX power cap | 10mW (TEST_MODE) / 100mW (production), set at first flash, not runtime-togglable | Implemented, `task_safety_guard.c`, ADR-004 |
| Emergency-service awareness | S3-side passive monitor flags proximity to P25/TETRA/DMR/FirstNet traffic; **device is made aware, operator is notified — it does not silently block, per ADR-003** | Partially implemented: RP2350B-side block-and-log path exists (`BLOCK_ES_OVERLAP`); the S3-side detection task and the UART-bridge override-confirmation dialog referenced in `task_safety_guard.c`'s own comments are not yet built |
| Aviation / ATC bands | Hardcoded exclusion — passively monitored and notified, never a target of active interference | Policy decision, ADR-003; enforcement follows the same not-yet-complete path as emergency-service awareness above |
| Append-only audit log | Every safety-relevant event (kill-switch state, block reason, TX allow) pushed to a TRNG-seeded, SHA-256 hash-chained log | Implemented, `audit_logger.c` / `task_safety_guard.c` |
| Mid-transmission cut | Kill switch or ES alert appearing mid-TX cuts RF VCC within one 50ms task tick, without waiting for the next TX request | Implemented, `task_safety_guard.c` |
| Geo-fence GPS zone locking | Infrastructure referenced in the project brief (zone DB v4.0) | **Not yet implemented in this codebase** — infrastructure-ready only, no enforcement code exists in the current tree |
| Proximity-based lock/wipe (SEC-002) | BLE RSSI paired-device proximity; LOCK/WIPE/LOCK_THEN_WIPE/ALERT_ONLY modes; software AES-256-GCM via mbedTLS, key via HKDF-SHA256 seeded by hardware TRNG, key in RP2350B OTP | Implemented, `task_proximity_guard.c`, ADR-015. **Correction**: this is software AES, not hardware — RP2350B has hardware SHA-256/TRNG/OTP/TrustZone but no AES peripheral. Any prior reference to "hardware AES" or "TrustZone AES encryption" for this feature is wrong and has been corrected. |

## 3. Known gaps (stated plainly, not glossed over)

- The emergency-service/aviation notify-not-block path has its RP2350B-side
  gate implemented, but the actual S3-side RF detection of P25/TETRA/DMR/
  FirstNet traffic, and the Flipper-UI override-confirmation dialog that
  `task_safety_guard.c` assumes exists (`task_uart_bridge.c`), are not built
  yet. Until they are, ES overlap simply blocks by default with no
  operator-facing override UI — which is *safer*, not less safe, but it
  means the "Advanced Security Professional" override-with-warnings mode
  described in the project brief does not exist in code yet.
- Geo-fencing is policy-level infrastructure, not implemented code.
- No independent third-party safety audit has been performed. This document
  is an internal engineering summary, not a compliance certificate.

## 4. Who should rely on this document

Security researchers and their legal/compliance reviewers assessing whether
the device's *engineering* safeguards are credible, ahead of the separate
legal question of what activity is lawful in their jurisdiction (covered in
`docs/LEGAL_DISCLAIMER.md`). This document will be revised as the gaps in
§3 are closed — each closure should reference the relevant ADR and be
reflected here in the same commit, not left to drift.

## References
- `docs/LEGAL_DISCLAIMER.md`
- `docs/decisions/ADR-003-es-monitor-notify-not-block.md`
- `docs/decisions/ADR-004-test-mode-first-flash.md`
- `docs/decisions/ADR-015-proximity-cutoff.md` (SEC-002)
- `firmware/rp2350b_master/src/task_safety_guard.c`
- `firmware/rp2350b_master/src/task_proximity_guard.c`
- `firmware/rp2350b_master/src/audit_logger.c`
