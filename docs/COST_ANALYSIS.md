# COST_ANALYSIS.md — Neuro-Sniffer Pro v0.3.3.0

Status: pre-alpha. Figures below are real BOM line items from `hardware/BOM.csv`
(Spine board, prototype quantities). Assembly, test, enclosure, and packaging
costs are explicitly marked TBD — there is no alpha board yet (per the project
brief), so no real assembly quote exists. Nothing in this document is invented
to fill that gap.

## 1. Component cost (from hardware/BOM.csv)

| Category | Ref(s) | Qty | Unit (USD) | Line total (USD) |
|---|---|---|---|---|
| ESP32-C5-WROOM-1-N4 (RF frontend) | U1 | 1 | 4.80 | 4.80 |
| ESP32-S3-WROOM-1-N16R8 (AI co-processor) | U2 | 1 | 5.50 | 5.50 |
| RP2350B-QFN56 (master controller) | U3 | 1 | 1.20 | 1.20 |
| E22-900M22S / SX1262 (Sub-GHz) | U4 | 1 | 3.80 | 3.80 |
| PN532 (NFC/RFID) | U5 | 1 | 2.50 | 2.50 |
| NEO-M9N-00B (GNSS) | U6 | 1 | 6.00 | 6.00 |
| TPS2115A (power mux) | U7 | 1 | 0.90 | 0.90 |
| MT3608 (boost converter) | U8 | 1 | 0.60 | 0.60 |
| Kill switch (SS12D00-G3) | SW1 | 1 | 0.40 | 0.40 |
| Connectors (USB-C, Flipper header, SMA, OLED header, microSD) | J1–J5 | 5 | — | 2.00 |
| RF shield cans (C5 + S3) | Shield1–2 | 2 | 1.20 | 2.40 |
| Passives (decoupling caps, inductor, pull-ups) | C1–30, L1, R1–10 | 41 | — | 1.00 |
| Bare PCB (4-layer, 65×35mm, ENIG black, 5-off prototype rate, PCBWay) | — | 1 | 2.20 | 2.20 |
| **Component + bare-PCB subtotal** | | | | **$34.30** |

This is the exact total in `hardware/BOM.csv`, ex-assembly/shipping, at prototype
(5-off) quantities. It does not include labour, SMT assembly, enclosure, battery
(PowerPack board, not yet on a combined BOM), packaging, or freight.

## 2. What's genuinely costed vs. what isn't

**Costed (this document, from the BOM):**
- All silicon, RF front-end parts, connectors, shielding, and passives for the
  Spine-equivalent build above.
- Bare PCB fabrication at PCBWay prototype rate.

**Not yet costed — explicitly TBD, not estimated:**
- SMT assembly labour (PCBWay quote requested only once Gerbers are final —
  see `hardware/PCBWAY_PROMPT.md` and the PCB-005/PCB-006 Trello cards).
- PowerPack board BOM (2500mAh LiPo, BQ25895, HUSB238, second SX1262,
  AS3911B) — tracked separately, not in `hardware/BOM.csv` yet.
- Enclosure/case tooling and material cost.
- Test fixture / bench-test labour time.
- Freight, duties, and packaging.
- Volume pricing at anything beyond the 5-off prototype rate.

## 3. Retail price context

The README's competitive table lists a target retail price of **$89.99** for
the full-featured unit. Against the $34.30 known component subtotal above,
that leaves roughly $55.69 to cover assembly, the PowerPack BOM, enclosure,
margin, and support — none of which are real numbers yet. Treat $89.99 as the
go-to-market target the project is pricing toward, not a bottom-up costed
retail price. This document will be updated with a real margin breakdown once
the first PCBWay assembly quote and the PowerPack BOM both exist.

## 4. Gating items before this doc can carry real margin figures

1. First PCBWay assembly quote (alpha board, per `hardware/PCBWAY_PROMPT.md`).
2. PowerPack board BOM finalised and costed.
3. Enclosure material/tooling quote.

## References
- `hardware/BOM.csv` — source of all figures above.
- `README.md` — competitive pricing table.
- `hardware/PCBWAY_PROMPT.md`, `hardware/PCBWAY_KICAD_AI_PROMPT_SPINE.md` — manufacturing prompts.
