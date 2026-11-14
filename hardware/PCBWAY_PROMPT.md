# PCBWAY_PROMPT.md — Neuro-Sniffer Pro v3.2.0
# AI Manufacturing Prompt for PCBWay (and equivalent: JLCPCB, Seeed Fusion)
# Use this text verbatim as input to PCBWay AI quoting/ordering system
# github.com/robotard/neuro-sniffer-pro

---

## HOW TO USE THIS FILE

1. Go to pcbway.com → Quick Order → PCB Instant Quote
2. Upload your Gerber files (generated from KiCad/EasyEDA export)
3. In the "Special Requirements" or "AI Notes" field, paste the relevant
   prompt section below for your board variant
4. For SMT assembly: upload BOM_v3_2.csv and CPL (centroid) file alongside Gerbers

---

## PROMPT — SPINE BOARD (Board 1)

```
Project: Neuro-Sniffer Pro v3.2.0 — Spine Board
Quantity: 5 (alpha prototype run)
Board dimensions: 65mm × 35mm
Layer count: 4 layers
Copper weight: 2oz outer layers (Layer 1 and 4), 1oz inner layers
PCB thickness: 1.6mm
Surface finish: ENIG (Electroless Nickel Immersion Gold)
  — Required for reliable pogo pad contact on bottom face
Solder mask colour: Black (both sides)
Silkscreen colour: White (both sides)
Via specifications:
  — Standard signal vias: 0.3mm drill, 0.6mm pad
  — SMA mounting vias: 0.8mm drill, 1.4mm pad (high current path)
  — Pogo contact pads (bottom layer): 2.54mm pitch, 1.5mm diameter, ENIG finish,
    NO solder mask over pads (exposed copper + ENIG only)
  — SWD debug testpoints (bottom layer): 0.5mm pitch, 0.4mm diameter, ENIG
Special requirements:
  — 4-layer stack: Signal / GND / Power / Signal (standard RF stack)
  — Continuous ground pour on Layer 2 (no splits, no islands)
  — RF keepout zones observed per Espressif WROOM module datasheet (3mm all sides)
  — GPS antenna keepout: no copper pour on Layer 1 above NEO-M9N module area (25×25mm)
  — Edge-mount SMA connectors (×3): confirm pad dimensions match Amphenol 132289 footprint
  — USB-C receptacle: confirm pad dimensions match GCT USB4135 footprint
  — Board edge: 0.5mm routed chamfer on all corners
  — Impedance control: 50Ω single-ended on RF traces (Layer 1, referenced to Layer 2 GND)
    USB D+/D− differential pairs: 90Ω differential (Layer 1)
SMT Assembly (if ordering with assembly):
  — BOM file: BOM_v3_2_spine.csv
  — CPL file: CPL_spine.csv
  — ESP32-C5-WROOM-1U and ESP32-S3-WROOM-1U-N16R8: standard SMT placement
  — RP2040: standard SMT placement (QFN-56)
  — SX1262 (E22-900M22S): standard SMT placement
  — PN532: standard SMT placement
  — NEO-M9N: standard SMT placement (confirm pad pitch with u-blox NEO-M9N datasheet)
  — SMA edge-mount connectors: DO NOT SMT — hand-solder after board assembly
    (edge-mount SMA requires mechanical alignment, not suitable for pick-and-place)
  — GPIO 2×10 header: DO NOT SMT — hand-solder (through-hole)
  — Pogo contact pads: pre-tinned ENIG surface, no component — just pad exposed
```

---

## PROMPT — POWERPACK BOARD (Board 2)

```
Project: Neuro-Sniffer Pro v3.2.0 — PowerPack Board
Quantity: 5 (alpha prototype run)
Board dimensions: 65mm × 35mm
Layer count: 4 layers
Copper weight: 2oz outer layers (Layer 1 and 4), 1oz inner layers
  — 2oz required on power planes: BQ25895 SW node, LiPo charge path carry 1.5A sustained
PCB thickness: 1.6mm
Surface finish: ENIG
Solder mask colour: Black (both sides)
Silkscreen colour: White (both sides)
Via specifications:
  — Standard signal vias: 0.3mm drill, 0.6mm pad
  — Power vias (BQ25895 SW node, VBUS, VBAT): 0.5mm drill, 1.0mm pad (×4 via stitching)
  — Pogo pins on top face: 2.54mm pitch, spring pogo pins (NOT fixed pads)
    Specify: Mill-Max 0906-2-15-20-75-14-11-0 or equivalent
    Tip height: 1.5mm compressed, 3mm extended — mates with Spine bottom pogo pads
Special requirements:
  — 4-layer stack: Signal / GND / Power / Signal
  — Thermal relief on BQ25895 thermal pad (exposed pad, GND): 0.3mm spoke width,
    minimum 4 spokes, via array below pad (4× 0.5mm vias for heat dissipation)
  — LiPo battery connector: JST-PH 2.0mm 2-pin, confirm polarity marking on silkscreen
    POLARITY IS CRITICAL — mark B+ and B− explicitly on silkscreen
  — LiPo protection: confirm BQ25895 overcurrent and overtemperature protection verified
    in schematic review before manufacturing
  — AS3911B: LF coil connector JST-ZH 2-pin, labelled COIL on silkscreen
  — SSD1306 OLED: 4-pin FPC/FFC connector or through-hole header (specify in KiCad)
  — USB-C J_PD: USB PD 3.0 input port. GCT USB4135 or equivalent CC-pin compatible.
    HUSB238 must have CC1/CC2 connected to USB-C CC pins for PD negotiation.
SMT Assembly (if ordering with assembly):
  — BOM file: BOM_v3_2_powerpack.csv
  — CPL file: CPL_powerpack.csv
  — BQ25895: QFN-24, standard SMT (thermal pad requires solder paste on pad AND vias)
  — HUSB238: SOT-23 or QFN-16 (confirm package from HYNETEK datasheet)
  — AS3911B: QFN-32, standard SMT
  — SX1262 second module: standard SMT placement
  — Pogo pins: DO NOT SMT — hand-press-fit after board assembly (interference fit)
  — LiPo connector: hand-solder (through-hole, polarity critical)
  — SMA edge-mount (×1, Sub-GHz TX): DO NOT SMT — hand-solder
```

---

## GERBER EXPORT CHECKLIST (KiCad)

Before uploading to PCBWay:

```
[ ] Plot → Gerber: all copper layers, F.Mask, B.Mask, F.Silkscreen, B.Silkscreen,
    Edge.Cuts, F.Courtyard, B.Courtyard
[ ] Drill file: Excellon format, combined PTH/NPTH
[ ] Board outline in Edge.Cuts layer only (no copper on outline)
[ ] DRC passed: zero errors, zero unconnected nets
[ ] 3D view reviewed: no component overlaps, SMA connectors at board edge
[ ] Silkscreen reviewed: polarity markers on LiPo connector, all ICs labelled
[ ] RF keepout zones: verify empty copper around WROOM modules
[ ] GPS keepout: verify no copper above NEO-M9N antenna area
[ ] Pogo pad pitch: verify 2.54mm matches between Spine bottom and PowerPack top
```

---

## ALPHA ORDER SUMMARY

| | Spine × 5 | PowerPack × 5 | Total |
|---|---|---|---|
| PCB fabrication | ~$35 | ~$35 | ~$70 |
| SMT assembly | ~$120 | ~$95 | ~$215 |
| Components (LCSC/Mouser) | ~$210 | ~$150 | ~$360 |
| Shipping (DHL Express) | ~$25 | ~$25 | ~$50 |
| **Alpha batch total** | | | **~$695** |

Note: PCBWay assembly requires minimum quantities and lead times (typically 7–14 days
for assembled prototype batches). JLCPCB is faster for bare PCB fabrication (3–5 days)
but SMT assembly minimum quantities differ. For alpha: bare PCBs from JLCPCB,
SMT by hand or local assembly house is also viable.

---

*v3.2.0 | robotard | github.com/robotard/neuro-sniffer-pro*
