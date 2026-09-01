# NEURO-SNIFFER PRO — PROJECT BRIEF v0.3.3.0
# Pin this to project instructions. Replace all previous versions.
# github.com/robotard/neuro-sniffer-pro

---

## WHAT IT IS

A standalone wireless security pen testing tool built on a three-chip stack:

```
ESP32-C5-WROOM-1U        — RF frontend (dual-band Wi-Fi 6, BLE5, Zigbee/Thread)
ESP32-S3-WROOM-1U-N16R8  — AI co-processor (TFLite Micro, Sub-GHz, NFC/RFID)
RP2350B                  — Master controller + safety guardian
```

UI and control surface: Flipper Zero (Zero — NOT One. See ADR-007).

Pre-alpha — no board yet.

---

## THE CHIP IS ESP32-C5. NOT C3. NOT C6.

The C5 is the only ESP32 with genuine simultaneous 2.4GHz + 5GHz Wi-Fi 6.

---

## THE CONTROLLER IS RP2350B. NOT RP2040.

RP2350B adds hardware SHA-256, TRNG, TrustZone signed boot, 520KB SRAM,
hardware float, and glitch detection. Cost delta: ~$0.20/unit.

---

## THREE-BOARD MODULAR SYSTEM

```
Spine (65×35mm)   — Core board. Clips to Flipper Zero GPIO header.
PowerPack         — Adds LiPo, BQ25895, HUSB238 USB PD 3.0,
                    2nd SX1262, AS3911B LF+HF RFID. Pogo interface.
Sandwich          — Spine + PowerPack integrated single PCB.
Test Base         — INTERNAL ONLY. Never ships. See ADR-I-001.
```

---

## CORE PURPOSE

Stress testing mission critical wireless devices to identify, correct,
and nullify vulnerabilities before deployment. Devices where wireless
failure carries life-safety consequences cannot simply be recalled.

---

## CAPABILITY SUMMARY

- Dual-band Wi-Fi 6, Ghost Tracker (zero-TX CSI), ghost_scanner 3D mapping
- Multi-source RF fusion, RF fingerprinting, fuzzy_noise anomaly engine
- Sub-GHz 150–960MHz, NFC/RFID, GPS, six TFLite Micro models, OTA
- SEC-002 proximity cutoff (LOCK/WIPE/LOCK_THEN_WIPE/ALERT_ONLY)
- Hardware kill switch, TRNG audit chain, ES/aviation notify-not-block

---

## WHO IT'S FOR

Skilled security researchers and professionals operating legally within
their jurisdiction, conducting authorised assessments on behalf of
manufacturers and operators of mission critical connected devices.

---

## VERSIONING

```
NSP-0.3.3.0
├── HW-0.1.0   Pre-first-board
├── FW-0.3.3   RP2350B firmware
├── SW-0.2.0   Build scripts + Flipper Lua
├── ML-0.1.0   Pre-real-hardware training
├── SEC-0.3.0  RP2350B signing + TRNG audit log
└── DOC-0.3.3  ADR system established
```

v0.x.x until first board boots. HW-0.1.1 is earned, not given.

---

## ADR SYSTEM

All architectural decisions captured in docs/decisions/ADR-XXX.md.
Sensitive decisions in internal/decisions/ADR-I-XXX.md (never public).

---

## KNOWN OPEN ISSUES

| Issue | Workaround | ADR |
|-------|-----------|-----|
| IDFGH-17558: 5GHz CSI static | 2.4GHz only for alpha | ADR-006 |
| AS3911B LF coil tuning | Bench tune on first board | HARDWARE_DESIGN_REVIEW.md |
| fuzzy_noise firmware | Not yet implemented | BENCH_TEST_ALPHA.md §A |
| Signed boot | Disabled for alpha | ADR-I-002 |

---

## BUILDER

Rob Hendricks — Robustas Design & Engineering, London.
LinkedIn: robstahendricks | GitHub: robotard | Portfolio: cargocollective.com/robustas