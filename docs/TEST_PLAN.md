# TEST_PLAN.md — Neuro-Sniffer Pro v3.2.0
# General Alpha Test Plan — Internal Use Only
# github.com/robotard/neuro-sniffer-pro

---

## SCOPE AND ENVIRONMENT

This document covers general functional validation of the Neuro-Sniffer Pro alpha board.
Advanced, high-power, or potentially interference-capable testing is separated into
BENCH_TEST_ALPHA.md and must only be performed by qualified personnel in a controlled
RF environment (Faraday cage or RF-shielded lab).

All testing in this document is:
- Passive capture only (zero TX) OR
- Low-power TX on equipment you own in a shielded environment
- TEST_MODE=true required for all alpha testing
- TX power cap: 10mW maximum (enforced by firmware)

**Required before any test session:**
- [ ] Written permission for any active TX targets documented and on file
- [ ] Faraday cage or RF-shielded bench available for all TX tests
- [ ] Hardware kill switch confirmed functional (see POWER_ON checklist)
- [ ] SD card formatted and mounted
- [ ] GPS lock confirmed (minimum 4 satellites)

---

## ENVIRONMENT SETUP

### Range/Distance Defaults (TEST_MODE)
All TX range tests default to MINIMUM distance:
- Wi-Fi deauth: 1m maximum
- Sub-GHz TX: 0.5m maximum, inside shielded enclosure
- BLE: 1m maximum
- NFC/RFID: contact-range only

Range can be extended per test session via Flipper menu with explicit confirmation.
Each extension is logged to audit with timestamp and operator confirmation.

### TEST_MODE Checklist Display
Before each capability category, firmware presents a checklist:
```
[CATEGORY: WIFI_ACTIVE]
Before proceeding, confirm:
[ ] Target device/network is yours or covered by written permission
[ ] Operating inside RF shielded environment or Faraday cage
[ ] TX power cap confirmed: 10mW
[ ] Audit log running: [YES/NO]
[ ] Kill switch accessible and functional
[PROCEED] [ABORT]
```

---

## SECTION 1 — POWER-ON AND HARDWARE VALIDATION

### 1.1 Spine Board Power-On
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Connect Spine to Flipper GPIO | Flipper detects module via UART handshake | | |
| RP2350B boot message on Flipper | "NSP v3.2.0 SPINE OK" | | |
| C5 boot confirmed | RP2350B reports "C5 ONLINE" | | |
| S3 boot confirmed | RP2350B reports "S3 ONLINE" | | |
| TEST_MODE flag confirmed | Status bar shows [TEST] | | |
| Kill switch test | Physical switch cuts RF VCC, Flipper shows "RF OFFLINE" | | |
| Kill switch restore | RF VCC restored, C5+S3 re-enumerate within 5s | | |
| Battery status (if PowerPack) | Flipper status shows [BAT: XX%] | | |

### 1.2 PowerPack Detection (PowerPack SKU only)
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Pogo pin connection | RP2350B GPIO26 HIGH at boot | | |
| "POWERPACK DETECTED" message | Flipper notification at boot | | |
| PowerPack features enabled | AS3911B, SX1262_B, OTG charge visible in menu | | |
| OLED status display | Shows BAT%, charge rate, GPS fix, ES status | | |
| BQ25895 I2C comms | No I2C errors in TEST_MODE verbose log | | |

### 1.3 Antenna and RF Chain
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| 2.4GHz antenna RSSI | Known AP visible at expected RSSI (±5dBm) | | |
| 5GHz antenna RSSI | Known AP visible on 5GHz band | | |
| SX1262 Sub-GHz RX | Known 433MHz device detected passively | | |
| GPS fix acquired | NEO-M9N acquires fix within 90s outdoors | | |
| U.FL connectors seated | Visual and RSSI check — no loose connectors | | |
| Mechanical: antenna weight | Board does not flex at SMA connector under antenna weight | | |

---

## SECTION 2 — PASSIVE CAPTURE (ZERO TX)

All tests in this section involve no transmission. Legal in all jurisdictions.
No shielded environment required. No checklist confirmation needed.

### 2.1 Wi-Fi Passive Scan
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| 2.4GHz passive scan | Known SSIDs appear with correct RSSI | | |
| 5GHz passive scan | Known 5GHz SSIDs appear | | |
| Both bands simultaneously | APs on both bands shown in single scan | | |
| PCAP capture to SD | .pcap file written to /sd/captures/ | | |
| GPS-tagged PCAP | PCAP contains GPS coordinates in comment | | |

### 2.2 Ghost Tracker — Passive CSI (2.4GHz, IDFGH-17558 workaround active)
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| CSI capture starts | "CSI ACTIVE 2.4GHz 52-subcarrier" message | | |
| Motion detection | Wave hand at 3m — motion score >0.6 | | |
| No-motion baseline | Empty room — motion score <0.2 for 30s | | |
| Breathing detection | Seated subject at 2m — BPM reading 12–18 | | |
| CSI data to SD | /sd/csi/csi_YYYYMMDD_HHMMSS.bin written | | |
| ghost_motion.tflite inference | Model loads and runs without OOM or crash | | |
| TEST_MODE: confidence scores | Raw confidence [walk:0.12, breathe:0.87, static:0.01] visible | | |

### 2.3 RF Device Fingerprinting (Passive)
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Start fingerprinting | "RFP ENGINE ACTIVE" in Flipper status | | |
| Known device appears | CFO, clock skew, spectral flatness logged | | |
| MAC randomisation test | Same physical device with new MAC — fingerprint matches stored | | |
| GPS stamp on fingerprint | GPS coordinates in /sd/rfp/fingerprints.json | | |

### 2.4 BLE Passive Scan
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| BLE advertisement scan | Known devices appear with RSSI and name | | |
| BLE density count | Room occupancy estimate from advertisement density | | |
| ble_anomaly.tflite | Model loads and scores BLE frames | | |

### 2.5 Sub-GHz Passive Capture
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| 433MHz capture | Known 433MHz keyfob signal detected | | |
| 868MHz capture | Known 868MHz device detected (EU ISM) | | |
| subghz_class.tflite | Protocol classification runs on capture | | |
| IQ buffer to PSRAM | Capture stored losslessly in 8MB PSRAM | | |
| Replay from PSRAM | Captured signal replayed at correct timing (own device only) | | |

### 2.6 NFC/RFID Passive Read
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Mifare Classic 1K read | Known test card UID read correctly | | |
| NTAG215 read | Full page dump to SD | | |
| AS3911B LF 125kHz (PowerPack) | Known EM4100 keyfob UID read | | |
| T5577 R/W (PowerPack) | Write and verify on blank test card | | |
| Dump saved to SD | /sd/nfc/dump_YYYYMMDD.json written | | |

### 2.7 Multi-Source RF Fusion (Ghost Tracker Enhanced)
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Fusion engine starts | All four sources initialised: CSI, Sub-GHz, BLE, Zigbee | | |
| Environment model builds | Baseline RF profile written after 60s | | |
| Person presence (fused) | Confidence score higher than CSI alone | | |
| Anomaly detection | Novel signal triggers fuzzy_noise alert | | |
| TEST_MODE: fusion debug | Individual source weights and scores visible | | |

### 2.8 Emergency Service Monitor (Passive, Always-On)
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| ES monitor running | "ES_MONITOR ACTIVE" in boot log | | |
| Detection test | Play P25/TETRA audio near antenna → [⚠ ES SIGNAL] in status bar | | |
| No false positives | 30min passive scan in clean environment, zero ES alerts | | |
| ES log entry | Detection logged to /sd/audit/audit.log with timestamp + GPS | | |

---

## SECTION 3 — ACTIVE TX (SHIELDED ENVIRONMENT, WRITTEN PERMISSION REQUIRED)

> All tests in this section require:
> - Equipment inside Faraday cage or RF-shielded bench
> - Written permission for all target devices
> - TEST_MODE checklist completed and logged
> - TX power: 10mW cap enforced
> - Range: minimum distance defaults (see Environment Setup)

### 3.1 Wi-Fi Active (Shielded)
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Deauth (broadcast) | Own test AP shows deauth frames in monitor mode | | |
| Deauth (targeted) | Specific test client deauthenticated from own AP | | |
| Beacon flood | Custom SSIDs visible on Wi-Fi scanner | | |
| Evil twin | Cloned test SSID visible on scanner | | |
| PMKID capture | PMKID captured from own WPA2 AP without client | | |
| Handshake capture | 4-way handshake from own test client | | |

### 3.2 Sub-GHz Active (Shielded)
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| 433MHz TX | Own test receiver acknowledges signal | | |
| Rolling code replay | Own known-fixed-code device actuates at 0.5m | | |
| Frequency sweep | Sweep 433–434MHz, results logged to SD | | |

### 3.3 fuzzy_noise Stress Test (Shielded, Controlled)
See BENCH_TEST_ALPHA.md for full fuzzy_noise stress test procedure.
This involves controlled noise generation and is separated for safety reasons.

### 3.4 Neuro-Ducky BadUSB
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| USB-HID enumeration | RP2350B appears as keyboard on test PC | | |
| Template: enum_windows.nd | Runs on own test PC, output to SD | | |
| Category warning | Restricted category triggers legal warning dialog | | |
| Override flag | CATEGORY_EXECUTE with NEURO_OVERRIDE header runs with log entry | | |
| Audit log entry | Every Ducky execution logged with timestamp + GPS | | |

---

## SECTION 4 — OTA AND FIRMWARE

### 4.1 OTA Update
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Wi-Fi connect for OTA | C5 connects to saved network | | |
| Manifest fetch | GitHub Releases manifest fetched and parsed | | |
| SHA256 verification | Correct hash verified before download | | |
| OTA partition write | New firmware written to inactive partition | | |
| Watchdog rollback | Flash deliberately corrupted image → auto-rollback within 60s | | |
| Version confirm | Version string matches expected after successful OTA | | |

### 4.2 Flipper Firmware Flash (via RP2350B USB host)
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| DFU entry via CDC | Flipper enters DFU without manual button press | | |
| DFU manual fallback | Flipper prompted for manual BOOT+BACK → DFU proceeds | | |
| Chunk CRC verification | Flash proceeds chunk by chunk with CRC | | |
| Anti-brick: bad SHA256 | Deliberately bad file → abort before DFU entry | | |
| Version confirm after flash | RP2350B reads version string and confirms | | |

---

## SECTION 5 — RF VECTORS LIBRARY

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Base set loads | /sd/rf_vectors/base/ files parsed at boot | | |
| Protocol signature match | Known OOK device matched to signature from base set | | |
| User import (.nrfv) | Drop test .nrfv to /sd/rf_vectors/user/, appears in menu | | |
| Malformed .nrfv rejected | Malformed file logs error, no crash | | |

---

## SECTION 6 — AUDIT LOG INTEGRITY

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Log file created | /sd/audit/audit.log exists on first boot | | |
| TX events logged | Every TX command appears in log with GPS + timestamp | | |
| Log is append-only | Existing entries cannot be modified via Flipper menu | | |
| ES detections logged | ES alert appears in log | | |
| TEST_MODE flag in log | All entries tagged TEST_MODE:true | | |
| Ducky executions logged | Override flag usage logged with operator note | | |

---

*v3.2.0 | Internal alpha test document | Do not distribute*
*robotard | github.com/robotard/neuro-sniffer-pro*
