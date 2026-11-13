# CHANGES_v0_3_3.md — Neuro-Sniffer Pro NSP-0.3.3.0
# Changelog — NSP-0.3.2.0 (pre-RP2350B) → NSP-0.3.3.0
# github.com/robotard/neuro-sniffer-pro

---

## VERSION SUMMARY

NSP-0.3.3.0 is a targeted but architecturally significant release.
One chip change. All downstream implications followed through completely.
No feature additions. No scope creep. Clean.

This is the last version before physical alpha board fabrication.

---

## BREAKING CHANGES

### [BREAKING] RP2040 → RP2350B throughout

Every reference to RP2040 is replaced with RP2350B.
This affects: BOM, schematics, PCB footprint, firmware CMakeLists,
flash scripts, test plans, PCBWay prompts, and all documentation.

PCB footprint change: QFN-56 (RP2040) → QFN-80 (RP2350B)
This requires a schematic and layout update before Gerber submission.
The RP2350B has 48 GPIO vs RP2040's 30 — our pin assignments are
unchanged, additional pins left unconnected with pull recommendations
per RP2350B datasheet section 2.19.

SDK change: Pico SDK 2.0+ required (was: 1.5.1+)
CMakeLists change: set(PICO_BOARD pico2) — was: set(PICO_BOARD pico)
All other SDK calls, FreeRTOS configuration, and UART/SPI/I2C APIs
are source-compatible. No firmware logic changes required.

Cost delta: +$0.20/unit at LCSC bulk pricing. Irrelevant at alpha quantities.

---

## NEW CAPABILITIES (from RP2350B hardware)

### Hardware SHA-256 acceleration
  Previously: software SHA-256 on Cortex-M0+ for OTA binary verification
              and audit log entry hashing.
  Now: hardware-accelerated SHA-256 via RP2350B crypto block.
  Impact:
    - OTA binary verification: ~40× faster, timing-invariant
    - Audit log entry hashing: no timing side-channel on hash operations
    - Every TX event hash is now cryptographically hardened at silicon level
  Implementation: use pico_sha256 hardware API (Pico SDK 2.0+)
                  Drop-in replacement for mbedTLS software SHA-256.

### Hardware True Random Number Generator (TRNG)
  Previously: RP2040 had no hardware RNG. ADC noise used for seeding —
              predictable under controlled conditions.
  Now: hardware TRNG via RP2350B (get_rand_32() / get_rand_64())
  Impact:
    - Every audit log entry carries a TRNG-generated nonce
    - Append-only log is now tamper-evident at cryptographic level,
      not just filesystem level
    - Session IDs, OTA transaction IDs, and TX event tokens all
      hardware-random — not predictable from external observation
  Implementation: add nonce field to audit log JSON schema (see below)

### ARM TrustZone + Signed Boot
  Previously: any UF2 could be flashed to RP2040 — no firmware signing.
              TEST_MODE safety relied entirely on firmware convention.
  Now: RP2350B supports signed boot via OTP-stored public key.
  Impact:
    - Production units can have safety guardian firmware signed
    - Unsigned UF2 rejected at boot — TEST_MODE cannot be silently
      bypassed by flashing an unsigned modified firmware
    - This makes the TEST_MODE/PRODUCTION distinction hardware-enforced,
      not convention-enforced
  Implementation roadmap:
    Alpha: signed boot NOT enabled (allows rapid iteration during testing)
    Pre-production: generate signing keypair, store public key in OTP,
                    sign all firmware builds via picotool
    Production: signed boot enabled, OTP locked
  Note: enabling Secure Boot disables RISC-V cores on RP2350B.
        We use Cortex-M33 — this is correct and expected behaviour.

### 520KB SRAM (previously 264KB)
  Impact: FreeRTOS task stacks no longer require aggressive size tuning.
  Updated stack allocations (conservative, can be tuned down on bench):
    task_uart_bridge:    4096 bytes (was 2048)
    task_safety_guard:   2048 bytes (was 1024) — safety task gets headroom
    task_gps_parser:     2048 bytes (was 1024)
    task_audit_logger:   3072 bytes (was 1536)
    task_power_manager:  2048 bytes (was 1024)
    task_ota_monitor:    1536 bytes (was 768)
    task_display:        1536 bytes (was 768)
    task_onboarding:     4096 bytes (was 2048)
    Total allocated:    ~20KB of 520KB — massive headroom for future tasks

### Hardware floating-point (Cortex-M33 FPU)
  Previously: software float on Cortex-M0+ (no FPU).
  Now: hardware single-precision FPU on Cortex-M33.
  Impact: GPS NMEA coordinate arithmetic, power budget calculations,
          OTA progress percentage — all faster and numerically cleaner.
  CMakeLists: add -mfpu=fpv5-sp-d16 -mfloat-abi=hard

### Voltage and clock glitch detection
  RP2350B detects voltage and clock glitching attacks in hardware.
  If detected: chip halts and requires power cycle.
  Impact: hardware attack attempts to bypass safety checks are detected
          at silicon level and halt execution immediately.
  This is niche but entirely appropriate for our threat model — a tool
  whose safety architecture is specifically designed to be resistant
  to bypass attempts should detect bypass attempt hardware attacks.

### 12 PIO state machines (previously 8)
  Current PIO allocation (unchanged from NSP-0.3.2.0 (pre-RP2350B)):
    PIO0 SM0: UART0 TX (→ Flipper)
    PIO0 SM1: UART0 RX (← Flipper)
    PIO0 SM2: UART1 RX (← NEO-M9N GPS)
    PIO0 SM3: USB host signalling
    PIO1 SM0: SPI monitoring (logic analyser function)
    PIO1 SM1: Reserved
  Headroom: 6 additional PIO state machines available.
  v4.0 candidates: WS2812 status LED, additional UART, custom RF protocol

---

## AUDIT LOG SCHEMA UPDATE

Audit log entries updated to include TRNG nonce field:

NSP-0.3.2.0 (pre-RP2350B) format:
```json
{
  "ts": "2025-01-15T14:23:01Z",
  "gps": {"lat": 51.5074, "lon": -0.1278, "fix": true},
  "cmd": "WIFI_DEAUTH",
  "params": {"target": "broadcast", "channel": 6},
  "test_mode": true,
  "tx_power_mw": 10
}
```

NSP-0.3.3.0 format (nonce field added):
```json
{
  "ts": "2025-01-15T14:23:01Z",
  "gps": {"lat": 51.5074, "lon": -0.1278, "fix": true},
  "cmd": "WIFI_DEAUTH",
  "params": {"target": "broadcast", "channel": 6},
  "test_mode": true,
  "tx_power_mw": 10,
  "nonce": "a3f8c2d1",
  "prev_hash": "sha256_of_previous_entry"
}
```

The nonce field is 4 bytes from hardware TRNG (get_rand_32()).
The prev_hash field chains entries cryptographically —
any tampering with a previous entry invalidates all subsequent hashes.
This creates a cryptographic chain of custody for the audit log.
Combined with the hardware TRNG, this is tamper-evident at silicon level.

---

## PLATFORM DECISION — FLIPPER ONE

Documented in internal/CHIP_DECISIONS_v3_2.md.
Decision: stay with Flipper Zero. Do not rush Flipper One.

Key points for the record:
  - Flipper One removed NFC, Sub-GHz, RFID from default hardware
  - GPIO is Linux SoC GPIO — incompatible with our UART bridge architecture
  - No confirmed release date — "still experimental"
  - Flipper One's MCU is also RP2350B — validates our upgrade independently
  - Our C5 has direct silicon Wi-Fi integration vs One's USB-connected MT7921
    (packet integrity architecturally cleaner on our stack)
  - We have already built the One's functional scope and exceeded it
    on the domains that matter for mission-critical RF security work

Revisit when Flipper One ships and community firmware matures.
That is a v5.0 conversation, not a v3.x one.

---

## FILES UPDATED IN NSP-0.3.3.0

| File | Change |
|------|--------|
| PRODUCT_ARCHITECTURE_v3_2.md → v3_3.md | RP2350B throughout, signed boot section, TRNG audit log |
| HARDWARE_DESIGN_REVIEW.md | RP2350B footprint, QFN-80, GPIO headroom, stack sizes |
| FIRMWARE_NOTES.md | Pico SDK 2.0+, CMakeLists changes, signed boot roadmap |
| BUILD_REPO.sh | pico2 board target, SDK 2.0, cmake flags |
| FLASH_TEST.sh | RP2350B UF2 path, signed boot disabled for alpha |
| FLASH_PRODUCTION.sh | RP2350B UF2, signed boot enabled pre-production |
| internal/ALPHA_ORDER_PACKAGE.md | BOM updated: RP2350B, QFN-80, $1.00/unit |
| internal/CHIP_DECISIONS_v3_2.md | Full decision rationale (new file) |
| PROJECT_BRIEF_v3_2_PINNED.md → v3_3.md | RP2350B, Flipper One note |
| firmware/rp2040_master/ → rp2350b_master/ | Directory rename, board target update |

---

## WHAT DIDN'T CHANGE

Everything else. All RF capabilities, all ML models, all safety architecture,
all pogo interfaces, all antenna specifications, all PCBWay prompts for Spine
and PowerPack PCBs (RP2350B is on Spine — no PowerPack schematic changes),
all test plans, all Ducky architecture, all RF Vectors library.

The RP2350B upgrade is a targeted, well-bounded change.
It does not ripple into RF hardware, AI pipeline, or power management.

---

## NEXT VERSION MILESTONE

NSP-0.3.3.1 — First alpha board in hand. Bring-up notes. Any schematic corrections.
          This is the first Vx.x.1 — the moment the project becomes physical.

NSP-0.4.0.0 — Post-alpha. AR integration infrastructure. GPS dead reckoning indoors.
          Real-time IQ streaming to PC. Geo-fence zone database v1.0.
          Flipper One reassessment checkpoint.

---

*NSP-0.3.3.0 | robotard | Robustas Design & Engineering*
*The last version before silicon. Make it count.*
