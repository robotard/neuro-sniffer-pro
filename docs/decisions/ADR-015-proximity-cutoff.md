# ADR-015 — Proximity-Based Device Lock/Wipe (SEC-002)

- Status: Accepted
- Date: 2026-11-16 (backfilled — implementation in `task_proximity_guard.c`
  and the Trello SEC-002 card predate this ADR; the decision itself was made
  earlier in the project but never written up as a numbered ADR, which is
  the gap this document closes)
- Domains: SEC, FW
- Tags: [security, proximity, ble, crypto-shred, rp2350b]
- References public: ADR-002 (RP2350B), ADR-009 (TRNG nonce chain)

## Context

A field unit that's lost, stolen, or left unattended during an authorised
engagement is a real exposure risk: captured RF Vectors, audit logs, and
any locally-cached test data could leak client information. The device
needed a way to detect separation from its paired operator and respond —
without inventing cryptography RP2350B doesn't actually have hardware
support for.

## Decision

Implement BLE RSSI-based proximity detection between the device and a
paired operator device (phone/Flipper companion). Four configurable
response modes:
- `LOCK` — device locks, requires re-authentication to resume.
- `WIPE` — crypto-shred of local sensitive data immediately.
- `LOCK_THEN_WIPE` — lock first, wipe after a configurable grace period
  if proximity isn't restored.
- `ALERT_ONLY` — notify the operator, take no destructive action.

Crypto-shred mechanics: destroy the encryption key first (instant,
irreversible), then background-overwrite the underlying data (best-effort,
not load-bearing for the security guarantee — the key destruction is what
actually matters).

**Encryption: software AES-256-GCM via mbedTLS.** RP2350B has hardware
SHA-256, a hardware TRNG, OTP storage, and TrustZone — it does **not** have
a hardware AES peripheral. An earlier version of this project's
documentation and Trello cards incorrectly described this as "hardware AES
via TrustZone." That was wrong and has been corrected everywhere it was
found (`task_proximity_guard.c`'s own header comment, Trello DOC-NEW-C and
DOC-012, and `docs/SAFETY_COMPLIANCE.md`). TrustZone provides execution
isolation, not an AES accelerator; the actual encryption is done in
software.

Key management:
- Key derived via HKDF-SHA256, seeded by the hardware TRNG.
- Key stored in RP2350B OTP (one-time-programmable, so a wipe of the
  derived working key is irreversible even if the overwrite pass is
  interrupted).

Remote kill: authenticated MQTT/SMS message, relayed through the ESP32-C5's
Wi-Fi task as a one-way request flag. The Wi-Fi task cannot invoke
crypto-shred directly — it can only set a flag that `task_proximity_guard.c`
reads and acts on. This keeps the same discipline as the control/data-plane
split in ADR-017: a radio-facing task is never itself the authority to
destroy keys or transmit; it can only request.

## Alternatives Considered

- **Hardware AES via RP2350B TrustZone**: rejected — doesn't exist.
  RP2350B has no AES peripheral. Including this here explicitly so the
  mistake has a documented correction to point to, not just a quiet fix.
- **GPS-based geofencing instead of BLE proximity**: deferred, not
  rejected — geofencing is referenced in the project brief (zone DB v4.0)
  as separate infrastructure, not a replacement for proximity-based
  lock/wipe. No geofence enforcement code exists in this tree yet.
- **Full data wipe (not crypto-shred)**: rejected for response time —
  crypto-shred is near-instant (destroy one key) vs. a full overwrite pass
  taking meaningfully longer on flash storage; the background overwrite
  still happens, it's just not the thing standing between "separated from
  operator" and "data is unreadable."

## Consequences

- Crypto-shred's security guarantee rests entirely on the OTP-stored key
  being genuinely destroyed/inaccessible after a wipe event — this is the
  component that most needs independent review before shipping to anyone
  relying on it for client data protection.
- Software AES-256-GCM has a real (if modest) performance cost on RP2350B
  vs. a hardware accelerator; not expected to be a bottleneck for this
  use case (key operations, not bulk data encryption), but noted here in
  case bulk-encryption needs ever change that calculation.
- The remote-kill path depends on network connectivity (MQTT/SMS via C5's
  Wi-Fi) — a unit that's offline cannot be remote-killed, only proximity
  can trigger a response in that case.

## References
- `firmware/rp2350b_master/src/task_proximity_guard.c` — implementation.
- `docs/SAFETY_COMPLIANCE.md` — safeguard status table, SEC-002 row.
- `docs/decisions/ADR-002-rp2350b-upgrade.md` — RP2350B's real hardware
  crypto capabilities (SHA-256, TRNG, OTP, TrustZone — no AES).
- `docs/decisions/ADR-017-hal-and-plane-split.md` — the request-not-authority
  discipline this ADR's remote-kill design follows.
