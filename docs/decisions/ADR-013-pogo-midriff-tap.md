# ADR-013 — Pogo Midriff Tap for Test Base
- Status: Accepted | Date: 2025-01-12 | Domains: HW-0.1.0
- Tags: [hardware, test-base, pogo, power, measurement]

## Context
Alpha TX power validation requires real LiPo + real BQ25895 + real Spine
under real load. Bench LDO does not replicate LiPo discharge behaviour
or BQ25895 dynamic load management accurately.

## Decision
Test Base: pogo arrays on BOTH faces. Shunts intercept each power rail
at the natural board boundary. USB-C bulkhead mirrors PowerPack USB-C
position exactly. J_PWR_SEL: MODE A (own power) or MODE B (real stack).

## Consequences
+ Real LiPo chemistry in TX power validation loop
+ Real BQ25895 dynamic load management observable
+ Production boards completely unmodified
+ USB-C position means charge cable never moves between modes
- Pogo arrays on both faces — slightly more complex PCB
