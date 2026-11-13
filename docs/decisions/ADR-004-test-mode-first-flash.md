# ADR-004 — Test Mode Set at First Flash Only
- Status: Accepted | Date: 2025-01-01 | Domains: SEC-0.2.0, FW-0.2.0
- Tags: [safety, test-mode, firmware, onboarding]

## Context
Device has two profiles: TEST (10mW, extended logging) and PRODUCTION (100mW).
Distinction must be meaningful and not trivially toggled.

## Decision
TEST_MODE set once at first flash via onboarding wizard. Stored in RP2350B NVS.
Not runtime-toggleable. Changing requires intentional re-flash or NVS clear.

## Alternatives Considered
**Runtime toggle**: Makes distinction meaningless. Rejected.
**Hardware switch**: Adds BOM cost and can be accidentally knocked. Rejected.

## Consequences
+ TEST_MODE is a meaningful designation
+ Every alpha unit clearly identified in audit logs
+ Production flash is a deliberate, documented act
- Slightly less convenient for developers switching modes
  Mitigated: re-flash takes ~3 minutes
