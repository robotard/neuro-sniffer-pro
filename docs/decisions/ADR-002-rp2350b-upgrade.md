# ADR-002 — RP2350B Upgrade from RP2040
- Status: Accepted | Date: 2025-01-15 | Domains: HW-0.1.0, FW-0.3.3, SEC-0.3.0
- Tags: [hardware, rp2350b, security, audit-log, signed-boot, trng]

## Context
Master controller handles safety-critical functions. A security tool's
controller should have hardware security primitives matching its role.
RP2040 lacks these. RP2350B adds them at ~$0.20/unit delta.
Flipper One independently chose RP2350B — external validation.

## Decision
RP2350B replaces RP2040 throughout. Pico SDK 2.0+. Board target: pico2.

## Alternatives Considered
**Keep RP2040**: $0.20/unit cheaper. Rejected: software SHA-256 has timing
side-channels; no hardware RNG means audit log nonces are predictable; no
secure boot means safety guardian can be silently replaced.
**Pico 2 module**: Too large for 65x35mm Spine. Bare RP2350B QFN-80 correct.

## Consequences
+ Hardware SHA-256: audit log hashing timing-invariant
+ Hardware TRNG: nonce chain cryptographically sound (ADR-009)
+ TrustZone: signed boot — safety firmware cannot be silently replaced
+ 520KB SRAM: FreeRTOS tasks no longer squeezed
+ Hardware float: GPS coordinate arithmetic cleaner
+ Glitch detection: hardware attack attempts on safety logic detected
- QFN-80 replaces QFN-56 — PCB footprint update required
- Signed boot adds keypair management overhead (see ADR-I-002)

## References
- ADR-009: TRNG nonce chain | ADR-I-002: Signed boot keypair (internal)
- docs/FIRMWARE_NOTES.md: Pico SDK 2.0 migration
