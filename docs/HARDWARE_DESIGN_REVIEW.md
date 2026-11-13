# HARDWARE_DESIGN_REVIEW.md — Neuro-Sniffer Pro v0.3.3.0
# Hardware decisions, open items resolved, case design, antenna notes
# github.com/robotard/neuro-sniffer-pro

---

## CHIP SELECTION — FINAL

### ESP32-C5-WROOM-1U (RF Frontend)
**Not C6. Not C3. C5.**

| | C3 | C6 | C5 |
|---|---|---|---|
| 2.4GHz Wi-Fi | ✓ 802.11n | ✓ 802.11ax | ✓ 802.11ax |
| 5GHz Wi-Fi | ✗ | ✗ | **✓ 802.11ax** |
| Both bands simultaneously | ✗ | ✗ | **✓** |
| BLE | 5.0 | 5.0 | **5.0** |
| Zigbee/Thread | ✗ | ✓ | **✓** |
| CSI quality rank | 4th | 2nd | **1st** |
| Core speed | 160MHz | 160MHz | **240MHz** |
| ESP-IDF support | v4.4+ | v5.1+ | **v5.5+** |
| Mass production | 2021 | 2023 | **April 2025** |

The -1U suffix denotes the U.FL/IPEX external antenna variant.
This is the correct variant. The non-U version has a PCB trace antenna that
creates keepout zone conflicts at our board dimensions.

**U.FL resistor bridge (open item — now resolved):**
The WROOM-1U module ships with a 0Ω resistor bridging the antenna matching
network to the U.FL connector. Confirm the resistor is on the U.FL path
(not the PCB trace path) by visual inspection under magnification before
soldering. Espressif module revision R3.1 and later: confirmed U.FL default.
Request module revision certificate from supplier (LCSC/Mouser) at order time.

### ESP32-S3-WROOM-1U-N16R8 (AI Co-processor)
N16R8 = 16MB flash, 8MB PSRAM. This specific variant is mandatory.
- 8MB PSRAM holds full IQ capture buffers (lossless Sub-GHz replay)
- 16MB flash accommodates all 6 TFLite models + firmware + SPIFFS
- N8R8 (8MB flash) is insufficient — models alone require ~750KB + firmware
- -1U suffix: external U.FL antenna, same keepout rationale as C5

### RP2350B
RP2350B (not RP2040 — see ADR-002). Hardware SHA-256, TRNG, TrustZone
signed boot, 520KB SRAM, glitch detection — all load-bearing on the
safety architecture, not optional extras. Cost delta ~$0.20/unit.
SWD debug pads accessible on underside of Spine PCB (0.5mm pitch testpoints).

---

## PCB DESIGN — SPINE (65 × 35mm)

### Layer Stack (4-layer recommended for RF performance)
```
Layer 1 (Top):    Signal + RF components + ESP32 modules
Layer 2:          Ground plane (continuous, critical for RF)
Layer 3:          Power planes (3.3V_MAIN, 3.3V_RF, VBUS)
Layer 4 (Bottom): Signal + pogo pad array + debug testpoints
```

4-layer is non-negotiable for dual-band RF at these frequencies.
2-layer boards will have unacceptable RF performance and ground bounce.

### Component Placement Priority
```
1. ESP32-C5-WROOM-1U    — Top-left quadrant, U.FL at board edge
2. ESP32-S3-WROOM-1U    — Top-right quadrant, U.FL at board edge
3. RP2350B              — Centre, with SWD testpoints accessible from bottom
4. SX1262 (E22-900M22S) — Bottom-left, SMA connector at board edge
5. PN532                — Bottom-right (Spine only — PowerPack has AS3911B)
6. NEO-M9N              — Centre-bottom, ceramic patch antenna facing up/out
7. Power path (AP2112K, TPS2115A) — Between GPIO header and chips
8. GPIO header (2×10, 2.54mm) — Right edge, matching Flipper Zero pinout
9. Pogo pad array (2×16, 2.54mm) — Bottom face, aligned for PowerPack mating
10. USB-C J1            — Bottom edge, right side
```

### RF Keep-Out Zones
- ESP32-C5 module: 3mm clearance all sides (per Espressif WROOM-1U datasheet)
- ESP32-S3 module: 3mm clearance all sides
- SX1262 antenna pin: 5mm clearance on antenna side
- NEO-M9N patch: top of board only, no copper pour above GPS antenna
- U.FL connectors: 10mm of clear space in cable path direction

### Critical Traces
- USB-C D+/D−: 90Ω differential pair, matched length ±0.5mm
- SPI buses (C5↔S3, S3↔SX1262, S3↔PN532): <50mm trace length, ground poured between
- I2C (RP2350B↔BQ25895): 4.7kΩ pull-ups to 3.3V, <100mm
- UART0 (RP2350B↔Flipper GPIO): 3.3V logic, series 33Ω termination
- Kill switch RF VCC path: rated for 1.5A continuous, 2A peak (2oz copper pour)

---

## CASE DESIGN — SPINE

### Overall Dimensions
```
Flipper Zero body:          100 × 40 × 25mm
Our board:                   65 × 35 × 8mm
Case (Spine only):           65 × 40 × 38mm (Flipper + our board + clip)
Case (Spine + PowerPack):    65 × 40 × 48mm
```

### Material
Primary: **Nylon PA12 SLS (3D printed, Sculpteo/Shapeways for proto)**
Production: Injection moulded ABS or polycarbonate (v1.0+)
Wall thickness: 2.5mm minimum (structural), 3.5mm at SMA mount point

PA12 SLS chosen for alpha because:
- No support removal issues (complex internal channel geometry)
- Good mechanical strength for SMA/antenna loads
- EMI-neutral (no metal fill to interfere with RF)
- Easy to prototype, dimensionally stable

### Antenna Mounting — Critical
The SMA connector carries the antenna's weight dynamically (vibration, flex during use).
Poor SMA mounting will cause the PCB to crack at the SMA footprint.

**SMA connector**: Edge-mount SMA (Amphenol 132289 or equivalent)
- PCB footprint: 4-point mounting (not 2-point)
- Additional mechanical anchor: epoxy bond to PCB at SMA body after soldering
- Case SMA port: through-case bulkhead fitting with lock-nut, NOT surface flush-mount
  The bulkhead fitting takes the mechanical load OFF the PCB
  Force path: antenna → SMA plug → bulkhead fitting → case wall → nothing to PCB

**U.FL pigtail to SMA bulkhead:**
- 100mm RG178 pigtail from C5 U.FL to SMA bulkhead (2.4/5GHz dual-band)
- 100mm RG178 pigtail from S3 U.FL to second SMA bulkhead (2.4GHz)
- 150mm RG178 pigtail from SX1262 to Sub-GHz SMA bulkhead
- All pigtails: strain-relief loop inside case (10mm radius minimum)
- Pigtails routed through internal channels, not free-floating

**Antenna weight loads at SMA:**
- Dual-band dipole (2.4/5GHz): ~8g typical → load on bulkhead fitting: negligible
- Sub-GHz whip (433–960MHz, 17cm): ~12g → moment arm significant when held at angle
- GPS patch: mounted flat to case top, no SMA, JST-ZH connector — no load concern
- LF coil (PowerPack): embedded in case lid foam, no SMA — no load concern

**Stress test before alpha:** Mount all antennas and apply 500g lateral load at antenna tip
for 30 seconds. Board must show zero flex or SMA movement. If any movement: increase
bulkhead wall thickness or add second epoxy anchor point.

### Internal Channel — USB-C Pigtail
```
Channel dimensions: 6mm wide × 4mm deep
Location: right inner face of case, running 25mm from board top to Flipper USB-C port
Cable: 100mm flat USB-C 2.0 (3mm wide, 1.5mm thick) — fits with 1mm clearance
Cable secured: two silicone retention clips at 10mm and 20mm from top
Flipper USB-C connector: permanently plugged, self-aligns when Flipper is seated in clip
```

### Flipper Zero Clip Mechanism
Flipper slides in from the top. Clip engages on Flipper's left and right rails.
GPIO header aligns to Flipper's top GPIO pads as the Flipper is seated.
USB-C pigtail self-inserts into Flipper USB-C as final seating motion.
Retention: two silicone-tipped spring clips on left and right, 200g extraction force.
Flipper GPIO header contact force: 0.5N per pin (standard 2.54mm header spec).

### PowerPack Mating
Pogo pins on top face of PowerPack mate with Spine bottom pogo pads.
Spring force: 0.5N per pin × 16 pins = 8N total clamping force.
Securing: two M2×6 brass inserts in case walls, M2 nylon screws (non-conductive).
This holds the stack rigid under antenna vibration loading.

---

## OPEN HARDWARE ITEMS — RESOLVED

### 1. AS3911B LF Coil Design
The AS3911B reads LF cards (125kHz) via an external coil.
Coil embedded in the case lid (flat, facing outward toward card).

**Coil specification (starting point for bench tuning):**
- Wire gauge: 0.25mm enamelled copper (AWG30)
- Turns: 25 (starting point — tune for resonance at 125kHz)
- Dimensions: 50mm × 30mm rectangular, matched to case lid footprint
- Inductance target: ~22µH (resonates at 125kHz with AS3911B internal capacitance)
- DC resistance: <2Ω (25 turns × ~160mm avg perimeter ≈ 4m @ 0.053Ω/m for AWG30 ≈ 0.2Ω — well within spec)
- Connector: JST-ZH 2-pin to PCB, 150mm pigtail

**Tuning procedure on bench:**
1. Wind coil to 25 turns. Measure inductance with LCR meter (target 20–25µH).
2. Connect to AS3911B EVM or directly to AS3911B XTAL_OSC_MOD register.
3. Place EM4100 test card at 10mm distance. Read UID. Adjust turns ±3 until clean read at 20mm.
4. Document final turn count for production.

### 2. BQ25895 Inductor Selection
The BQ25895 switching charger requires an external inductor on its SW pin.

**Selected part:** Bourns SRR4028-4R7Y (or equivalent)
- Inductance: 4.7µH
- DC resistance (DCR): 42mΩ (well within <50mΩ requirement)
- Saturation current (Isat): 2.6A (exceeds our 1.5A max charge rate with margin)
- Package: 4.0 × 4.0 × 2.8mm — fits on PowerPack PCB

Alternative: Murata LQH44PN4R7MP0L (4.7µH, Isat 2.5A, DCR 48mΩ)
Both are acceptable. Bourns preferred for LCSC availability.

### 3. C5 WROOM-1U U.FL Resistor Bridge
Confirmed above (Hardware Selection section).
Action: request revision certificate at order time. Visual verify before assembly.

---

## BOM HIGHLIGHTS (Key Components)

| Component | Part | Supplier | Est. Cost (USD) |
|---|---|---|---|
| ESP32-C5-WROOM-1U | Espressif official | Mouser/LCSC | $3.80 |
| ESP32-S3-WROOM-1U-N16R8 | Espressif official | Mouser/LCSC | $4.20 |
| RP2350B | Raspberry Pi | LCSC/Mouser | $1.00 |
| SX1262 (E22-900M22S) | Ebyte | LCSC | $3.50 |
| BQ25895 | Texas Instruments | Mouser | $2.10 |
| HUSB238 | HYNETEK | LCSC | $0.90 |
| PN532 | NXP | LCSC | $1.80 |
| AS3911B | STMicroelectronics | Mouser | $3.40 |
| NEO-M9N | u-blox | Mouser | $12.00 |
| SKY65013-92LF LNA | Skyworks | Mouser | $1.20 |
| SSD1306 128×32 OLED | Generic | LCSC | $1.80 |
| BQ25895 inductor (Bourns SRR4028-4R7Y) | Bourns | Mouser | $0.45 |
| AP2112K-3.3 LDO | Diodes Inc | LCSC | $0.15 |
| TPS2115A power mux | Texas Instruments | Mouser | $0.85 |
| USB-C receptacle (J1) | GCT USB4135 | Mouser | $0.35 |
| SMA edge-mount (×3) | Amphenol 132289 | Mouser | $0.90 each |
| 2500mAh LiPo | Generic 1S flat cell | Alibaba | $4.50 |

**Spine COGS estimate: ~$38–42 assembled**
**PowerPack COGS estimate: ~$28–32 assembled (excl. Spine)**
**Sandwich COGS estimate: ~$58–65 assembled**

Full BOM with quantities and alternates: hardware/BOM.csv

---

*v0.3.3.0 | robotard | Robustas Design & Engineering | github.com/robotard/neuro-sniffer-pro*
