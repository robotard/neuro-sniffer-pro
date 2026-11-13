# ADR-007 — Flipper Zero Over Flipper One
- Status: Accepted | Date: 2025-01-15 | Domains: HW-0.1.0, SW-0.2.0
- Tags: [hardware, flipper, platform, ui]

## Context
Flipper One announced as successor. Decision: Zero, One, or both?

## Decision
Flipper Zero is the target platform. Reassess when One ships with
mature community firmware.

## Alternatives Considered
**Target Flipper One**: Removed NFC/Sub-GHz/RFID from default hardware.
Linux SoC GPIO incompatible with our UART bridge. No release date.
No community firmware. $300-500. Rejected on all counts.
**Target both**: Doubles maintenance before first board fabbed. Rejected.

## Consequences
+ Zero: shipping, 500k+ users, massive ecosystem
+ Our architecture maps cleanly to Zero's 2x10 GPIO header
+ Zero has NFC/Sub-GHz/RFID — we complement, not duplicate
+ Flipper One chose RP2350B as MCU — validates ADR-002
- If One ships and dominates: port needed
  Mitigated: Spine is the product, Flipper is the UI

## References
- internal/CHIP_DECISIONS.md: full Flipper One analysis
