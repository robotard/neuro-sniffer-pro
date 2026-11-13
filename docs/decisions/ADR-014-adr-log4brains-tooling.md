# ADR-014 — ADR Format + Log4brains for Open Source
- Status: Accepted | Date: 2025-01-15 | Domains: DOC-0.3.3
- Tags: [documentation, adr, log4brains, open-source]

## Context
Significant architectural decisions accumulated. At open source release,
contributors need to understand constraints without a two-hour onboarding.
Single version number flattens six domains with very different risk profiles.

## Decision
ADR format for all architectural decisions. Log4brains (npm) for public
open source release — searchable static site from ADR markdown.
Multi-domain versioning: HW/FW/SW/ML/SEC/DOC independent cadence.
Sensitive ADRs: internal/decisions/ — never public.

## Consequences
+ Every decision traceable to version and rationale
+ Log4brains generates browsable site at open source release
+ Internal/public split enforced structurally by directory
- Slight overhead per decision vs paragraph in existing doc
  Payoff at open source release and v1.0
