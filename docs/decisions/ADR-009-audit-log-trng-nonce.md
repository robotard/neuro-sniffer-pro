# ADR-009 — TRNG Nonce Chain in Audit Log
- Status: Accepted | Date: 2025-01-15 | Domains: SEC-0.3.0, FW-0.3.3
- Tags: [security, audit-log, trng, rp2350b, tamper-evident]

## Context
Audit log is the chain of custody for all TX events. Filesystem
append-only is necessary but not sufficient — a determined actor
bypassing filesystem layer could still modify entries undetected.
RP2350B (ADR-002) provides hardware TRNG.

## Decision
Every audit log entry: 4-byte hardware TRNG nonce + SHA-256 hash
of previous entry. Tamper-evident at silicon level.

## Alternatives Considered
**Filesystem append-only only**: Bypassable with direct flash access.
Insufficient for legal evidence use case. Rejected.
**External secure element (ATECC608)**: Maximum security but adds BOM
cost and I2C complexity. RP2350B TRNG achieves essential property
without external components. Rejected.

## Consequences
+ Tamper-evident at cryptographic level
+ TRNG ensures nonces genuinely unpredictable
+ Chain of custody provable in legal proceedings
+ Zero additional BOM cost
- Small per-entry overhead: 36 bytes/entry (8 nonce + 32 hash)
