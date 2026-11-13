# ADR-003 — Emergency Services Monitor: Notify Not Block
- Status: Accepted | Date: 2025-01-01 | Domains: SEC-0.2.0, FW-0.2.0
- Tags: [safety, emergency-services, es-monitor, firmware]

## Context
Device operates near ES bands. P25/TETRA/DMR/FirstNet are safety-critical.
Must demonstrate good-faith responsibility without impeding professionals.

## Decision
ES detection is an awareness tool, not a governor.
Passive: always-on, zero action. Active TX overlap: single dialog,
default STOP, 10s timeout. Hardware kill switch: ultimate physical override.

## Alternatives Considered
**Hard block**: Automatically halt on ES detection. Rejected: creates
false safety theatre, constantly interrupts professionals in legitimate
environments, does nothing to deter determined bad actors.
**No monitoring**: Rejected: we can add meaningful awareness at zero cost.

## Consequences
+ Professionals get awareness without friction
+ Every detection GPS+timestamp logged
+ Hardware kill switch remains ultimate physical safety layer
- Does not prevent determined misuse — acknowledged. Nothing does.
- MIT licence: checks removable by anyone with a compiler. Correct.
