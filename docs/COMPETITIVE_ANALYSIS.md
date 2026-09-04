# Competitive Analysis — Neuro-Sniffer Pro v0.3.3.0

## Feature Matrix
| Feature | Flipper Wi-Fi | Marauder | HackRF One | **Neuro-Sniffer Pro** |
|---|---|---|---|---|
| Price | $29 | $30-50 | $340 | **$89.99** |
| 5GHz Wi-Fi 6 | ✗ | ✗ | ✗ | **✓** |
| Dual-band simultaneous | ✗ | ✗ | ✗ | **✓** |
| Sub-GHz 150-960MHz | ✗ | ✗ | ✓ | **✓** |
| AI on-device | ✗ | ✗ | ✗ | **✓** |
| NFC/RFID | ✗ | ✗ | ✗ | **✓** |
| GPS wardrive | ✗ | ✗ | ✗ | **✓** |
| Ghost Tracker (CSI) | ✗ | ✗ | ✗ | **✓** |
| RF Fingerprinting | ✗ | ✗ | ✗ | **✓** |
| Passive triangulation | ✗ | ✗ | ✗ | **✓** |
| OTA + auto-rollback | ✗ | ✗ | ✗ | **✓** |
| Hardware kill switch | ✗ | ✗ | ✗ | **✓** |
| Emergency shield | ✗ | ✗ | ✗ | **✓** |
| BadUSB | ✗ | ✗ | ✗ | **✓** |
| Open source | ✗ | ✓ | ✓ | **✓** |

## Cost to Replicate With Individual Products
Alfa AWUS036ACH (5GHz) $45 + HackRF One $340 + ACR122U NFC $40 + GPS $30 + Jetson Nano AI $130 + mmWave motion $55 + custom integration ~$100 = **$740+**
**Neuro-Sniffer Pro: $89.99.**

## C5 vs C6 — Final Word
C6: single-band 2.4GHz, 160MHz, CSI rank 2nd. C5: dual-band 2.4+5GHz, 240MHz, CSI rank 1st.
The C5 is the only correct choice for this project. The C6 is not.

## Ghost Tracker — Sensing Without Transmitting
- Motion: ~95% accuracy at 5m through one wall. Published ESP32 CSI research, 2024-2025.
- Breathing: Goertzel filter 9-36 BPM. ~90% at rest.
- Person ID from static CSI: ~99.82% (transformer model, 2025 research).
- Triangulation: ESPARGOS passive channel charting, Berlin 6G Conference 2024. ~0.5-1.5m NLOS.
- RF fingerprinting: carrier frequency offset + clock skew + spectral flatness. MAC-random resistant.