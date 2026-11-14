# PCBWay AI — Generate KiCad From Scratch — Spine Board (Alpha Test Board)
# Neuro-Sniffer Pro v0.3.3.0 | THE GATE — first board, validates everything
# Paste the block under "AI DESIGN PROMPT" verbatim into PCBWay AI / Flux.ai / KiCad AI.
# github.com/robotard/neuro-sniffer-pro

---

## WHY THIS BOARD FIRST

Spine is the complete standalone product (£99.99). PowerPack and Sandwich are
second-phase expansions built once Spine is bench-validated. This prompt exists
to take us from zero to a submittable KiCad project without hand-drawing the
schematic — the AI tool does placement + routing, we correct DRC and antenna
keepouts by hand afterward.

---

## ORDER PARAMETERS

    Dimensions: 65 x 35mm | Layers: 4 | Thickness: 1.6mm | Material: FR4 TG150
    Surface finish: ENIG (mandatory — RF performance + pogo pad reliability)
    Solder mask: Black | Silkscreen: White
    Impedance control: YES — 50Ω single-ended, 90Ω differential
    Min track/space: 0.1/0.1mm | Min drill: 0.2mm | Qty: 5 (alpha run)
    Stackup: L1(signal) / L2(solid GND) / L3(solid 3.3V PWR) / L4(signal)

---

## AI DESIGN PROMPT

```
Design a 4-layer PCB for Neuro-Sniffer Pro v0.3.3.0 Spine board,
65x35mm, clips onto a Flipper Zero GPIO header. This is the alpha
test board — first board fabricated for this project.

Stackup: Top(signal) / L2(solid GND) / L3(solid 3.3V PWR) / Bottom(signal).

═══════════════════════════════════════════════════════════
POWER
═══════════════════════════════════════════════════════════
  USB-C J1 VBUS(5V) → AP2112K-3.3 LDO U8 → 3.3V_USB rail
  Flipper J2 pin1(3.3V) → 3.3V_FLIPPER rail
  TPS2115A U7: input-A=3.3V_FLIPPER, input-B=3.3V_USB,
    auto-selects higher/present source → 3.3V_MAIN rail
  Kill switch SW1 in series with 3.3V_MAIN feed to:
    U1(C5) VCC, U4(SX1262) PA_VCC, U9(SKY65013 LNA) VCC only
    — RF-only cutoff. RP2350B, S3, GPS stay powered for logging.
  100nF decoupling within 0.5mm of every VCC pin.
  10uF bulk within 3mm of each power domain (USB, Flipper, main).

═══════════════════════════════════════════════════════════
RP2350B U3 — master controller (QFN-80, NOT RP2040 QFN-56)
═══════════════════════════════════════════════════════════
  GPIO0/1     → UART0 TX/RX ↔ Flipper Zero J2 GPIO0/1 (33Ω series term)
  GPIO2/3/4   → SPI0 SCK/MOSI/MISO (shared bus, RP2350B is SPI master)
  GPIO5       → SPI0 CS → C5 U1 (command channel)
  GPIO6       → SPI0 CS → S3 U2 (command channel)
  GPIO8/9     → UART1 TX/RX ↔ NEO-M9N U6 (GPS NMEA, RX only used)
  GPIO10/11   → I2C0 SDA/SCL → reserved (PowerPack OLED when mated)
  GPIO14-17   → SPI1 SCK/MOSI/MISO/CS → microSD J5
  GPIO20      → C5 heartbeat input (OTA watchdog, 1Hz toggle expected)
  GPIO21      → S3 RESET output (active LOW)
  GPIO22      → C5 RESET output (active LOW)
  GPIO26      → PowerPack detect input (pulled LOW, HIGH when PowerPack pogo mated)
  GPIO27      → Emergency input from S3 (HIGH = ES detected, kill TX)
  GPIO28      → Kill switch sense input (pulled-up, HIGH = engaged = TX permitted)
  USB_DP/DM   → USB-C J1 D+/D- (Neuro-Ducky HID mode, 90Ω diff pair)
  SWD test pads (bottom face, no connector, ENIG open, 0.5mm dia,
    0.5mm pitch) — probe-only points for alpha bring-up via Test Base

═══════════════════════════════════════════════════════════
ESP32-C5-WROOM-1U U1 — RF frontend (-1U = U.FL variant, NOT PCB trace)
═══════════════════════════════════════════════════════════
  VCC → 3.3V_MAIN via kill switch SW1
  SPI slave: GPIO4(CS) GPIO5(CLK) GPIO6(MOSI) GPIO7(MISO) ← RP2350B
  UART0: GPIO16/17 → S3 UART (OTA relay path, C5↔S3 only)
  RESET ← RP2350B GPIO22 | BOOT: 10k pull-up (download-mode jumper optional)
  ANTENNA: U.FL connector on module (confirm 0Ω bridge on U.FL path,
    NOT PCB-trace path — verify against module rev cert before placement).
    U.FL pigtail (100mm RG178) → SMA bulkhead J6 (dual-band 2.4/5GHz).
    Component-body RF keepout: 3mm all sides, no copper/vias on any layer.

═══════════════════════════════════════════════════════════
ESP32-S3-WROOM-1U-N16R8 U2 — AI co-processor (N16R8, NOT N8R8)
═══════════════════════════════════════════════════════════
  VCC → 3.3V_MAIN, always-on (not gated by kill switch)
  SPI slave: GPIO5(CS) GPIO6(CLK) GPIO7(MOSI) GPIO8(MISO) ← RP2350B
  SPI→SX1262: GPIO10(CLK) GPIO11(MOSI) GPIO12(MISO) GPIO13(CS)
  SX1262 BUSY→GPIO18, DIO1→GPIO19, RST→GPIO20
  SPI→PN532: GPIO14(CLK) GPIO15(MOSI) GPIO16(MISO) GPIO17(CS)
  Emergency out: GPIO4 → RP2350B GPIO27 (ES detection trigger)
  UART0: GPIO43/44 ← C5 (OTA relay path)
  RESET ← RP2350B GPIO21 | BOOT: 10k pull-up
  ANTENNA: U.FL connector on module (-1U variant).
    U.FL pigtail (100mm RG178) → SMA bulkhead J7 (2.4GHz secondary).
    Component-body RF keepout: 3mm all sides.

═══════════════════════════════════════════════════════════
SX1262 (E22-900M22S) U4 — Sub-GHz primary RX/TX
═══════════════════════════════════════════════════════════
  VCC → 3.3V_MAIN via kill switch SW1 (PA power gated with C5)
  SPI/BUSY/DIO1/RST ← S3 as above
  RF_OUT → 50Ω trace → pi-network (3x 0402, values per E22 datasheet)
    → LNA U9 (SKY65013-92LF, RX path only, +18dB gain) → SMA bulkhead J3
  Antenna: 150mm RG178 pigtail, SMA J3, wideband 150-960MHz whip.

═══════════════════════════════════════════════════════════
PN532 U5 — HF NFC/RFID (Spine only — PowerPack replaces with AS3911B)
═══════════════════════════════════════════════════════════
  SEL0=1 SEL1=0 (SPI mode select)
  SPI/IRQ/RST ← S3 as above

═══════════════════════════════════════════════════════════
NEO-M9N U6 — GNSS
═══════════════════════════════════════════════════════════
  UART TX→RP2350B GPIO9 | UART RX←RP2350B GPIO8
  3mm keepout radius, NO copper pour on Layer 1 in 25x25mm area
    above module — L2/L3/L4 ground pour permitted (shielding).
  Onboard ceramic patch antenna (ANN-MB-00 pattern) — bench alpha
    can substitute external active patch via test point if needed.

═══════════════════════════════════════════════════════════
POGO PAD ARRAY — PowerPack mating interface (bottom face)
═══════════════════════════════════════════════════════════
  2x16 array, 2.54mm pitch, 1.5mm pad diameter, ENIG finish,
    NO solder mask over pads (exposed copper + ENIG only).
  Location: bottom face, centred, clear of all other bottom-face routing.
  Pin 1 alignment mark: silkscreen triangle, top-left corner of array
    (must match PowerPack top-face array orientation exactly).
  Carries: 3V3_MAIN, GND, I2C0 (SDA/SCL to PowerPack BQ25895+HUSB238),
    SPI bus (2nd SX1262 CS + shared CLK/MOSI/MISO), PowerPack detect
    signal (GPIO26), VBAT sense, OTG 5V passthrough.
  See docs/PRODUCT_ARCHITECTURE.md pogo pin table for full 16-pin map.

═══════════════════════════════════════════════════════════
GPIO HEADER J2 — Flipper Zero interface
═══════════════════════════════════════════════════════════
  2x10, 2.54mm pitch, through-hole, matches Flipper Zero GPIO pinout.
  45° bevel on this board edge for reliable Flipper header mating.

═══════════════════════════════════════════════════════════
ROUTING RULES
═══════════════════════════════════════════════════════════
  RF traces (C5→SMA, S3→SMA, SX1262→LNA→SMA): 50Ω microstrip,
    ~0.75-0.85mm wide on this stackup (verify with fab impedance report).
    No 90° bends (45° or curved only). No vias on RF traces.
    Pi-network matching on SX1262 RF output before LNA.
  USB-C D+/D-: 90Ω differential, length-matched ±0.1mm.
  SPI buses (RP2350B↔C5, RP2350B↔S3, S3↔SX1262, S3↔PN532):
    length-matched ±2mm within each bus, max 50mm total trace length.
  I2C0: max 50mm total, 4.7kΩ pull-ups to 3.3V_MAIN near RP2350B.
  UART (Flipper, GPS, C5↔S3 OTA relay): standard single-ended, 33Ω
    series termination on Flipper-facing UART0 only.
  GND strategy: split RF_GND (under U1, U4, U9) from DGND (under
    U2, U3, U5, U6). Join at single star point under U3 via 0402
    0Ω link — prevents RF return current coupling into digital domain.
  Via stitching: every 2mm along RF traces and around all RF components.
  Keepouts: 3mm component-body around C5 and S3 modules (all layers),
    3mm radius above NEO-M9N (Layer 1 only — pour permitted below).

═══════════════════════════════════════════════════════════
MECHANICAL
═══════════════════════════════════════════════════════════
  65x35mm board outline, 0.5mm chamfer all four corners.
  4x M2 mounting holes (1.0mm drill) at corners for case assembly —
    do NOT populate at factory, through-hole only.
  SW1 kill switch: right edge, accessible from outside finished case.
  SMA bulkheads J3 (Sub-GHz), J6 (C5 dual-band), J7 (S3 2.4GHz):
    right edge, evenly spaced, barrel flush with board edge for
    case bulkhead-fitting mount (mechanical load goes to case,
    NOT to PCB — see docs/HARDWARE_DESIGN_REVIEW.md antenna load path).
  J2 Flipper GPIO header: left edge, flush, 45° bevel.
  J1 USB-C: right edge, bottom half.
  J5 microSD: top edge, near U3.
  Pogo pad array: bottom face centre (see above).

OUTPUT: Gerbers (RS-274X), Drill (Excellon), BOM (CSV), Pick-and-Place
(CSV), Assembly PDF, STEP model. Request impedance report before
fab approval — confirm 50Ω/90Ω achieved on this stackup.
```

---

## PRE-SUBMIT CHECKLIST (after AI generates the layout)

```
[ ] Confirm C5 and S3 both show U.FL pads populated (NOT on-module trace
    antenna footprint — this is the single most common AI-tool error
    for -1U variant modules, since most training data assumes standard
    WROOM trace-antenna parts)
[ ] Confirm three SMA bulkheads present: J3 (Sub-GHz), J6 (C5), J7 (S3)
[ ] Confirm pogo pad array present on bottom face, 2x16, ENIG open,
    no solder mask over pads
[ ] Confirm RP2350B footprint is QFN-80 (not QFN-56 — that's RP2040)
[ ] Run DRC — zero errors, zero unconnected nets
[ ] 3D view: no component overlaps, SMA connectors flush at board edge
[ ] Request impedance calculation report from PCBWay before approving fab
[ ] Cross-check every pin assignment above against
    docs/HARDWARE_DESIGN_REVIEW.md and hardware/SCHEMATIC_NOTES.md
```

---

## RECOMMENDED AI TOOLS

1. **PCBWay AI Board Designer** — native to the fab house, tightest
   DFM feedback loop, first choice for this alpha run.
2. **Flux.ai** — strong AI-native schematic capture + layout, good
   fallback if PCBWay's tool struggles with the -1U antenna footprints.
3. **KiCad 8 + FreeRouting** — free, full manual DRC control, use this
   as the verification/correction pass regardless of which AI tool
   generates the first draft — always re-open in KiCad before submitting.

---

*v0.3.3.0 | robotard | Robustas Design & Engineering*
*Spine — the test board that gates everything else*
