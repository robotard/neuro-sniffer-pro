# ADR-012 — ML Models Closed Source for Alpha
- Status: Accepted (temporary) | Date: 2025-01-01 | Domains: ML-0.1.0, SEC-0.2.0
- Tags: [ml, open-source, security, models]

## Context
Six TFLite models encode learned RF behaviour. Premature open-sourcing
risks: unvalidated accuracy critique, fuzzy_noise evasion research,
community pressure outside curation standard.

## Decision
Models closed source (binary .tflite only) for alpha and pre-production.
Open sourced at v1.0. Training pipelines open at v0.9.0 for methodology review.

## Consequences
+ Time to validate on real hardware before community scrutiny
+ Prevents premature gaming of anomaly detection
- Reduces community trust during alpha/beta
  Mitigated: public commitment to open source at v1.0
