# ADR-008 — Three-Board Modular System
- Status: Accepted | Date: 2025-01-01 | Domains: HW-0.1.0
- Tags: [hardware, architecture, modular, sku, pogo]

## Context
Multiple market segments: professionals (full capability), entry-level
(Flipper clip-on), builders (bare PCBs). Single board can't serve all
at sane price.

## Decision
Three boards: Spine (65x35mm core), PowerPack (battery+expansion),
Sandwich (integrated). Pogo pin interface 2x16 2.54mm. Internal Test Base.

## Consequences
+ Entry price point £99.99 (Spine alone)
+ PowerPack expansion for existing Spine owners
+ Sandwich single clean assembly for professionals
+ Pogo interface: boards independently testable
- Three PCB designs to maintain
- Sandwich duplicates Spine+PowerPack functionally (intentional for form factor)
