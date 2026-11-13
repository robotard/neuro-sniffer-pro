# ADR-001 — ESP32-C5 Over C6 for Dual-Band Wi-Fi 6
- Status: Accepted | Date: 2025-01-01 | Domains: HW-0.1.0, FW-0.3.0
- Tags: [hardware, esp32, wifi, csi, rf-frontend]

## Context
RF frontend determines every Wi-Fi capability. Project requires simultaneous
dual-band Wi-Fi 6 for professional RF assessment of modern networks.

## Decision
ESP32-C5-WROOM-1U is the RF frontend. Not C3. Not C6.

## Alternatives Considered
**C6**: Single-band 2.4GHz only despite Wi-Fi 6 marketing. Rejected: 5GHz
non-optional for professional RF research. Most enterprise/medical networks
run primary traffic on 5GHz.
**C3**: 802.11n, single-band. Two generations behind. Rejected categorically.

## Consequences
+ Only ESP32 with genuine simultaneous dual-band Wi-Fi 6
+ Best CSI performer in ESP32 family — critical for Ghost Tracker
+ 240MHz core vs C6's 160MHz
+ Native Zigbee/Thread
- Mass production April 2025 — newer chip, fewer community examples
- ESP-IDF v5.5+ required — see IDFGH-17558 bug (ADR-006)

## References
- ADR-006: Ghost Tracker 2.4GHz workaround
- docs/FIRMWARE_NOTES.md: IDF requirements
