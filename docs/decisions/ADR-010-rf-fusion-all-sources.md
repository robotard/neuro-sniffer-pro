# ADR-010 — Multi-Source RF Fusion: All Four Inputs
- Status: Accepted | Date: 2025-01-10 | Domains: FW-0.3.2, ML-0.1.0
- Tags: [firmware, ml, ghost-tracker, fusion, csi, subghz, ble, zigbee]

## Context
Ghost Tracker CSI sensing degrades in low Wi-Fi environments.
Mission critical testing often in RF-quiet settings. Additional passive
RF sources can extend environmental awareness.

## Decision
All four RF sources feed the fusion engine:
CSI (primary motion), Sub-GHz passive (range extension),
BLE density (occupancy confidence), Zigbee/Thread (protocol context).

## Alternatives Considered
**CSI only**: Already implemented. Goes blind without Wi-Fi traffic.
Critical failure mode for mission critical use case. Rejected.
**CSI + Sub-GHz only**: Good but BLE/Zigbee add useful signals at
negligible additional cost (C5 already handles both). Rejected.

## Consequences
+ Significantly higher confidence than CSI alone
+ Effective in RF-quiet environments (critical for use case)
+ Sub-GHz adds ~2-3km passive range via LNA
+ Directly useful for search and rescue
- Four concurrent C5 tasks vs one — minor power budget increase
