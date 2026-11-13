# ADR-011 — Ducky Script Whitelist + Override Flag
- Status: Accepted | Date: 2025-01-01 | Domains: SEC-0.2.0, FW-0.2.0
- Tags: [safety, ducky, badusb, whitelist, audit]

## Context
RP2350B USB-HID allows arbitrary keystroke injection. Powerful for
professionals, dangerous for misuse. Must allow legitimate use while
creating friction for misuse and maintaining audit trail.

## Decision
Four whitelisted categories (no restriction): INFO, NETWORK, FILE_READ, AUDIT.
Four restricted (require override header): EXECUTE, MODIFY, PERSIST, EXFIL.
Override execution logged with operator note + GPS timestamp.

## Consequences
+ Whitelisted ops: zero friction for legitimate diagnostic use
+ Restricted ops: deliberate, documented, audited
+ Override flag creates clear operator intent record
- Determined misuse simply adds override flag
  Acknowledged: flag is not a lock, it is a mirror and a record.
