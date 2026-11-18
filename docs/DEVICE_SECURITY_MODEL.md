# DEVICE_SECURITY_MODEL.md — Neuro-Sniffer Pro v0.3.3.0

How the device protects itself and the data it holds — distinct from
`docs/SAFETY_COMPLIANCE.md` (which covers protecting *the RF
environment and third parties* from the device). This document covers
three things: proximity-based cutoff, the access-control model for
on-device functions, and the security posture of the external API.

**Correction carried from the Trello card history**: an earlier
description of this feature said "RP2350B TrustZone AES encryption."
That was wrong. RP2350B has hardware SHA-256, a hardware TRNG, OTP
storage, and TrustZone execution isolation — it has **no AES hardware
peripheral**. The actual encryption is software AES-256-GCM via
mbedTLS. This document reflects that correction throughout; see
`docs/decisions/ADR-015-proximity-cutoff.md` for the full decision
record, including why the wrong description happened.

## 1. Proximity cutoff (SEC-002)

**Mechanism**: BLE RSSI-based proximity detection between the device
and a paired operator device, implemented in
`firmware/rp2350b_master/src/task_proximity_guard.c`.

**Response modes** (configurable):
| Mode | Behaviour |
|---|---|
| `LOCK` | Device locks, requires re-authentication to resume |
| `WIPE` | Immediate crypto-shred of local sensitive data |
| `LOCK_THEN_WIPE` | Lock first; wipe after a configurable grace period if proximity isn't restored |
| `ALERT_ONLY` | Notify the operator, no destructive action |

**Crypto-shred mechanics**: the encryption key is destroyed first —
instant and irreversible, since it lives in RP2350B OTP (one-time
programmable: a destroyed key cannot be un-destroyed even if the
process is interrupted). A background overwrite of the underlying data
follows, but it's best-effort and not what the security guarantee
rests on — key destruction alone is sufficient to make the data
unrecoverable.

**Encryption**: software AES-256-GCM via mbedTLS. Key derived with
HKDF-SHA256, seeded by the hardware TRNG. Key stored in RP2350B OTP.

**Remote kill**: an authenticated MQTT/SMS message, relayed through the
ESP32-C5's Wi-Fi task as a one-way request *flag* — the Wi-Fi task
cannot invoke crypto-shred directly, it can only set a flag that
`task_proximity_guard.c` reads and acts on. This follows the same
"request, not authority" discipline as the control/data-plane split in
ADR-017: no radio-facing task is ever itself allowed to destroy keys or
key a transmitter.

**What this does NOT cover** (stated plainly, not glossed over):
- Geo-fence GPS zone locking is separate, policy-level infrastructure
  (zone DB v4.0) with no enforcement code in this tree yet — it is not
  part of proximity cutoff and shouldn't be assumed to back it up.
- No independent review of the OTP key-destruction guarantee has been
  performed. This is the single component that most needs scrutiny
  before anyone relies on it to protect client data in the field.

## 2. Access control framework

The device has no user account system — access control is physical and
mode-based, not identity-based:

- **Kill switch** (hardware, `task_safety_guard.c`) gates all RF
  capability regardless of what command is sent — see
  `docs/SAFETY_COMPLIANCE.md` for the full safety-architecture table.
- **TEST_MODE**, set once at first flash (ADR-004), permanently caps TX
  power for the life of that flash. Not a runtime permission — a
  manufacturing/provisioning-time decision.
- **"Advanced Security Professional" mode**, per the project brief,
  requires explicit activation plus multiple warning prompts (legal
  jurisdiction warnings, ethical warnings) before any restricted
  capability becomes reachable. **This mode is not implemented in the
  current firmware.** The only override-adjacent code that exists today
  is the Ducky/BadUSB "restricted category" override flag described
  below — nothing resembling the full Advanced Security Professional
  gate exists yet.
- **Ducky/BadUSB command whitelisting**: RP2350B USB-HID commands are
  grouped into categories (`INFO`, `NETWORK`, `FILE_READ`, `AUDIT` —
  whitelisted by default). Restricted categories require an override
  flag set in the script header, and every use of that flag is written
  to the audit log (ADR-011). This is the one access-control mechanism
  in the whole device that currently has a working override path with
  logging — everything else in this section is either hard-blocked
  (kill switch, frequency whitelist) or not yet built (Advanced
  Security Professional mode, ES-overlap override).
- **ES-band overlap**: per ADR-003, blocks by default with no override
  path implemented yet (see `docs/SAFETY_COMPLIANCE.md` §3 and the open
  Trello review ticket at
  [trello.com/c/SszYaYrD](https://trello.com/c/SszYaYrD) tracking the
  missing `task_uart_bridge.c` override-confirmation UI).

## 3. External API security model

See `docs/EXTERNAL_API_SPEC.md` for the full protocol. From a security
standpoint, the honest summary is: **the Flipper-facing UART protocol
has no authentication, no encryption, and no integrity check of its
own.** Its security currently comes entirely from the physical layer —
it's a point-to-point UART link between the Spine board and a
physically clipped-on Flipper Zero, not a network-exposed interface.
Anyone with physical access to the UART pins could send commands
directly. This is an accepted tradeoff for a physically-attached
accessory (the same trust model Flipper Zero's own GPIO modules use),
not an oversight — but it does mean the proximity-cutoff mechanism in
§1 is doing double duty as the device's only defense once the unit
itself is out of the operator's physical control, since the UART
protocol offers no independent gate of its own.

The internal C5↔S3↔RP2350B control-plane protocol
(`docs/INTERNAL_API_CONTRACT.md`) is a closed, on-board SPI bus — not
exposed externally — so the `safety_ctx` gating described there is a
safety mechanism (ensuring every TX-capable internal message carries
its test-scenario context) rather than an access-control boundary
against an external attacker. It should not be read as device security
in the sense this document covers.

## References
- `docs/decisions/ADR-015-proximity-cutoff.md`
- `docs/decisions/ADR-002-rp2350b-upgrade.md` (RP2350B's real crypto hardware)
- `docs/decisions/ADR-003-es-monitor-notify-not-block.md`
- `docs/decisions/ADR-004-test-mode-first-flash.md`
- `docs/decisions/ADR-011-ducky-whitelist-override.md`
- `docs/decisions/ADR-017-hal-and-plane-split.md`
- `docs/EXTERNAL_API_SPEC.md`
- `docs/INTERNAL_API_CONTRACT.md`
- `docs/SAFETY_COMPLIANCE.md`
- `firmware/rp2350b_master/src/task_proximity_guard.c`
- `firmware/rp2350b_master/src/task_safety_guard.c`
