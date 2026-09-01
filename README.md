# Neuro-Sniffer Pro v0.3.3.0
### AI-Driven RF Security Research Platform for Flipper Zero

> Active testing requires explicit written authorisation. Passive monitoring
> (Ghost Tracker, ghost_scanner 3D mapping, wardrive scan) is zero-TX and
> lawful in most jurisdictions. Emergency service and aviation bands are
> never blocked — this device makes the operator aware of proximity to
> them so a test never accidentally interferes. See ADR-003.

## Why This Exists
| Product | Price | 5GHz | AI | Sub-GHz | NFC | GPS |
|---|---|---|---|---|---|---|
| Flipper Wi-Fi module | $29 | ✗ | ✗ | ✗ | ✗ | ✗ |
| Marauder ESP32 | $30-50 | ✗ | ✗ | ✗ | ✗ | ✗ |
| HackRF One | $340 | ✗ | ✗ | ✓ | ✗ | ✗ |
| **Neuro-Sniffer Pro** | **$89.99** | **✓** | **✓** | **✓** | **✓** | **✓** |

## Hardware
| Chip | Role |
|---|---|
| ESP32-C5-WROOM-1U | RF frontend — Wi-Fi 6 dual-band 2.4+5GHz, BLE5, Zigbee, #1 CSI performer in ESP32 family |
| ESP32-S3-WROOM-1U-N16R8 | AI co-processor — TFLite Micro, SX1262 Sub-GHz, PN532 NFC, 8MB PSRAM |
| RP2350B | Master — Flipper bridge, safety guardian, TRNG audit chain, OTA orchestrator |

**C5 not C6**: C6 is single-band 2.4GHz. C5 is dual-band 2.4+5GHz. ESP-IDF v5.5+.
**RP2350B not RP2040**: hardware SHA-256, TRNG, TrustZone — see ADR-002.

## AI / ML Features (6 on-device models)
rf_classifier · ghost_motion · crowd_count · fuzzy_noise · ble_anomaly ·
subghz_class — plus `ghost_scanner` (3D area scanning via multi-unit CSI).

## Security
SEC-002 proximity cutoff: paired-device BLE proximity, LOCK/WIPE/
LOCK_THEN_WIPE/ALERT_ONLY. See docs/decisions/ADR-015.

## Quick Start
```
git clone https://github.com/robotard/neuro-sniffer-pro.git
cd neuro-sniffer-pro
bash scripts/INITIAL_SETUP.sh      # or BUILD_REPO.sh / BUILD_REPO_MAC.sh from anywhere
bash scripts/FLASH_TEST.sh         # alpha bench flash, safety checklist gated
```

## OTA Updates
Flipper → System → Check for Updates → dual-bank write → 60s auto-rollback

## Docs
Start at `docs/decisions/ADR-000-index.md` for every architecture decision.

## Licence
Firmware: MIT | Models: CC-BY-NC-SA | No cloud | No telemetry