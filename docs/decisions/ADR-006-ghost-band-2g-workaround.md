# ADR-006 — Ghost Tracker 2.4GHz Band (IDFGH-17558 Workaround)
- Status: Accepted (temporary) | Date: 2025-01-01 | Domains: FW-0.3.0
- Tags: [firmware, csi, ghost-tracker, idf-bug, 5ghz]

## Context
ESP-IDF v5.5.4 bug IDFGH-17558: 5GHz CSI buffers on C5 are statically
allocated and do not update. Ghost Tracker on 5GHz returns frozen data.
2.4GHz CSI unaffected.

## Decision
Ghost Tracker operates 2.4GHz only for alpha.
GHOST_BAND compile flag defaults to WIFI_BAND_2G.
Superseded when IDFGH-17558 confirmed fixed.

## Alternatives Considered
**Block alpha until 5GHz fixed**: Delays indefinitely for one feature subset.
Rejected: 2.4GHz Ghost Tracker fully functional and valuable.
**Ship with broken 5GHz enabled**: Silent incorrect data worse than
known limitation. Rejected categorically.

## Consequences
+ Alpha proceeds on schedule with correct 2.4GHz CSI
+ Single compile flag re-enables 5GHz when patch lands
- 5GHz Ghost Tracker unavailable for alpha

## References
- docs/FIRMWARE_NOTES.md: patch paths A, B, C
- firmware/esp32_c5_rf/src/ghost_tracker.c: GHOST_BAND flag
