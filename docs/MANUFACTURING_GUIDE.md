# MANUFACTURING_GUIDE.md — Neuro-Sniffer Pro v0.3.3.0

Handoff document for an assembly house (PCBWay or equivalent) producing
Spine boards beyond hand-built prototype quantities. **Pre-alpha notice:**
this describes the intended handoff package; it has not yet been exercised
against a real production run — no alpha board exists yet. Treat line items
marked TBD as genuinely open, not as placeholders with an implied answer.

## 1. What the assembly house receives

- Gerbers + drill files (RS-274X), generated from the KiCad project built
  from `hardware/PCBWAY_KICAD_AI_PROMPT_SPINE.md`.
- Pick-and-place (centroid) file.
- Bill of materials: `hardware/BOM.csv`, with manufacturer part numbers and
  approved sources (LCSC / DigiKey as listed per line).
- `hardware/PCBWAY_PROMPT.md` — the manufacturing brief (board spec: 4-layer,
  65×35mm, ENIG finish, black soldermask).
- This document, for process/test requirements below.

## 2. Board spec summary (from BOM + PCBWay prompt)

| Spec | Value |
|---|---|
| Layers | 4 |
| Dimensions | 65mm × 35mm |
| Surface finish | ENIG |
| Soldermask | Black |
| Prototype quantity costed | 5-off |

## 3. Components requiring special handling

- **RF shield cans** (Shield1, Shield2 — Laird SMD-50-55-1): fit after reflow,
  per standard RF-can placement; do not reflow the cans themselves through
  the main oven profile if the assembly house's process risks resoldering
  the parts underneath.
- **ESP32-C5-WROOM-1 / ESP32-S3-WROOM-1 modules**: castellated-edge modules,
  standard module reflow profile per Espressif's reflow guidelines — not the
  bare-die profile.
- **RP2350B QFN56**: fine-pitch QFN, no exposed thermal pad connections
  called out in the BOM beyond standard — follow Raspberry Pi's QFN56
  land-pattern and paste-stencil recommendations.

## 4. Test points required before shipment

Every unit must pass, at minimum, before leaving the assembly house (exact
fixture TBD — no bed-of-nails or boundary-scan fixture has been designed
yet, this is a gating item):

1. Continuity: no short between 3V3 and GND.
2. Power-on self-test: `NEURO:READY` string on UART0 at 115200 baud within
   5 seconds of power application (per `firmware/rp2350b_master/src/main.cpp`).
3. Kill-switch fail-safe check: with the kill switch physically OFF, RF VCC
   (GPIO16, per `task_safety_guard.c`) must read 0V. This is the single
   most important go/no-go test on the line — a unit that fails this must
   not ship.
4. `SAFETY_STATUS` UART command returns a well-formed JSON reply.

## 5. Firmware flashing at the factory

Production units are flashed via `scripts/FLASH_PRODUCTION.sh`, which sets
the 100mW TX power cap (vs. 10mW in `scripts/FLASH_TEST.sh`) and locks
TEST_MODE at first flash per ADR-004 — this is not a runtime-togglable
setting, so it must be set correctly at the factory flashing step, not left
for the end customer to configure. Signed boot (ADR-I-002, internal-only)
remains **disabled** until the pre-production signing key process is
complete — do not enable signed boot at the factory without that internal
runbook in hand.

## 6. Open items before a real production run (TBD, not estimated here)

- Bed-of-nails or boundary-scan test fixture design.
- Assembly-house quote and lead time (requires final Gerbers).
- PowerPack board (separate BOM, not yet merged into a combined production
  BOM with the Spine board).
- Packaging and labelling (regulatory markings — see `docs/LEGAL_DISCLAIMER.md`
  for the jurisdictional disclaimers that must accompany every unit).

## References
- `hardware/BOM.csv`, `hardware/PCBWAY_PROMPT.md`, `hardware/PCBWAY_KICAD_AI_PROMPT_SPINE.md`
- `scripts/FLASH_PRODUCTION.sh`
- `docs/decisions/ADR-004-test-mode-first-flash.md`
- `internal/decisions/ADR-I-002-signed-boot-keypair.md` (internal only)
