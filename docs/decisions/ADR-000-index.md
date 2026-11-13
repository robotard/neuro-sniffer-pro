# ADR-000 — Decision Index — Neuro-Sniffer Pro v0.3.3.0

| ADR | Title | Status | Domains |
|-----|-------|--------|---------|
| ADR-001 | ESP32-C5 not C6 | Accepted | HW, FW |
| ADR-002 | RP2350B upgrade from RP2040 | Accepted | HW, FW, SEC |
| ADR-003 | ES monitor — notify not block | Accepted | SEC, FW |
| ADR-004 | Test Mode first-flash only | Accepted | SEC, FW |
| ADR-005 | RF Vectors — identify not exploit | Accepted | SEC, DOC |
| ADR-006 | Ghost Tracker 2.4GHz IDFGH-17558 | Accepted | FW |
| ADR-007 | Flipper Zero not One | Accepted | HW, SW |
| ADR-008 | Three-board modular system | Accepted | HW |
| ADR-009 | TRNG nonce chain in audit log | Accepted | SEC |
| ADR-010 | Multi-source RF fusion all four inputs | Accepted | FW, ML |
| ADR-011 | Ducky whitelist + override flag | Accepted | SEC, FW |
| ADR-012 | ML models closed source alpha | Accepted | ML, SEC |
| ADR-013 | Pogo midriff tap test base | Accepted | HW |
| ADR-014 | ADR format + Log4brains | Accepted | DOC |
| ADR-016 | Git as source of truth | Accepted | DOC, FW |
| ADR-017 | HAL role naming + control/data plane split | Accepted | FW, SEC |
| ADR-I-001 | Test base JTAG/SWD (INTERNAL) | Accepted | HW |
| ADR-I-002 | Signed boot keypair mgmt (INTERNAL) | Accepted | SEC |

Template: see bottom of this file.
Internal ADRs: internal/decisions/ — never pushed to public repo.

---
## ADR TEMPLATE
```
# ADR-XXX — Title
- Status: Proposed | Accepted | Deprecated | Superseded by ADR-YYY
- Date: YYYY-MM-DD
- Domains: HW | FW | SW | ML | SEC | DOC
- Tags: [tag1, tag2]
## Context
## Decision
## Alternatives Considered
## Consequences
## References
```
