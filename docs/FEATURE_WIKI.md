# Feature Wiki — Neuro-Sniffer Pro v0.3.3.0

## ESP32-C5-WROOM-1 (RF Frontend)
C5 vs C6: C6=single-band 2.4GHz 160MHz. C5=dual-band 2.4+5GHz 240MHz, #1 CSI in ESP32 family. ESP-IDF v5.5+ required.

| Feature | Detail |
|---|---|
| Wi-Fi 6 dual-band | 2.4GHz ch1-14 + 5GHz UNII-1/2A/2C simultaneously |
| Promiscuous sniff | PMKID, WPA2/3 handshake, beacon, probe, data frames |
| Deauth | Broadcast / targeted / stealth (random src MAC) |
| Evil twin | Clone SSID + deauth to force reconnect |
| Beacon flood | Custom SSID list from SD |
| BLE 5 | Passive sniff, spoof, fuzz, flood, pairing bypass |
| Zigbee/Thread | 802.15.4 hardware radio (independent from Wi-Fi) |
| CSI Ghost Tracker | 52 subcarriers @ 20Hz, motion/breathing/presence |
| RF Fingerprinting | CFO + clock skew + spectral flatness (MAC-random resistant) |
| OTA host | Downloads C5+S3 firmware, relays S3 via UART bridge |

## ESP32-S3-WROOM-1-N16R8 (AI Co-processor)
| Feature | Detail |
|---|---|
| TFLite Micro | Xtensa LX7 vector extensions, 6 models <200KB each |
| SX1262 Sub-GHz | 150-960MHz FSK/LoRa/OOK, capture/replay/sweep |
| PSRAM capture | 8MB buffer, lossless IQ replay |
| Rolling code | Pattern analysis + de Bruijn sequence gen |
| PN532 NFC | Mifare Classic/NTAG/DESFire read/write/clone/brute |
| Emergency kill | GPIO4 HIGH → RP2350B kills all TX within 50ms |

## RP2350B (Master Controller)
| Feature | Detail |
|---|---|
| Flipper bridge | UART0 115200 8N1, all command routing |
| Safety guardian | FreeRTOS 50ms tick: kill switch + emergency + whitelist |
| OTA orchestrator | Triggers C5, monitors heartbeat, rollback detection |
| GPS parser | NEO-M9N NMEA → gps_last.json cache |
| OLED | SSD1306 128×64 spectrum + GPS map + status |
| Neuro-Ducky | USB-HID BadUSB from /sd/gui/ducky/ |
| Audit log | Every TX: timestamp+GPS+cmd, append-only |

## Safety Architecture
| Layer | Mechanism |
|---|---|
| Kill switch | Physical — cuts RF VCC. No software override. |
| Frequency whitelist | Rejects TX outside 300MHz-6GHz |
| Emergency shield | P25/TETRA/DMR/FirstNet → GPIO kill <50ms, notify not block |
| Geo-fence | GPS zone → TX disabled |
| Power cap | 10/50/100mW configurable |
| Audit log | Every TX event logged, cannot be disabled |

## ML Model Zoo
| Model | Task | Size | Accuracy |
|---|---|---|---|
| rf_classifier.tflite | 6-class signal ID | ~180KB | ~87% |
| ghost_motion.tflite | Walk/breathe/static | ~95KB | ~91% |
| crowd_count.tflite | Occupancy estimate | ~120KB | ~79% |
| fuzzy_noise.tflite | Anomaly detection | ~88KB | ~93% |
| ble_anomaly.tflite | BLE violations | ~110KB | ~85% |
| subghz_class.tflite | Sub-GHz protocol | ~145KB | ~82% |

Hosted: https://huggingface.co/neuro-sniffer-pro
Train custom: python3 software/pc_tools/train_model.py
Licence: CC-BY-NC-SA. No user RF data collected or transmitted.

## OTA Flow
1. Flipper → System → Check for Updates
2. RP2350B → C5 SPI: OTA_CHECK_VERSION
3. C5 fetches ota_manifest.json from GitHub Releases, saves to /sd/ota/
4. If newer: download .bin, verify SHA256, write to OTA partition 1
5. 60s watchdog boot validation → auto-rollback on failure
6. C5 relays S3 firmware via UART bridge (RP2350B drives S3 RESET+BOOT)