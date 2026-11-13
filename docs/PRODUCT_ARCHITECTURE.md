# Neuro-Sniffer Pro — Product Architecture v3.2
# "The Spine, The PowerPack, and The Sandwich"
# github.com/robotard/neuro-sniffer-pro
# Author: Robsta | Contributor: Claude

---

## PRODUCT PHILOSOPHY

We are an extension of the Flipper Zero. Not a competitor.
The Flipper is the UI, the case, the hands. We are the brain and the radio.
Every SKU requires a Flipper Zero. That is intentional and correct.

Primary market: security researchers and professionals conducting authorised
assessments of mission critical wireless devices — medical equipment, search
and rescue hardware, industrial control systems, and any device where wireless
failure has life-safety consequences.

Responsible use is the easy default path. Not the only path.

---

## CHIP STACK

### ESP32-C5-WROOM-1U — RF Frontend
- Dual-band Wi-Fi 6 (2.4GHz + 5GHz simultaneously) — the ONLY ESP32 variant
  with genuine simultaneous dual-band. C6 is single-band 2.4GHz only.
- Best CSI performer in the entire ESP32 family (C5 > C6 > C3 ≈ S3 > original)
- BLE 5, Zigbee, Thread (802.15.4 native)
- 240MHz RISC-V core
- -1U variant: factory U.FL/IPEX connector, eliminates antenna keepout complications
- ESP-IDF v5.5+ required. Known bug: IDFGH-17558 (5GHz CSI static buffers)
  → See FIRMWARE_NOTES.md for patched IDF path

### ESP32-S3-WROOM-1U-N16R8 — AI Co-processor
- Xtensa LX7 dual-core, 16MB flash, 8MB PSRAM
- Fastest TFLite Micro inference in the ESP32 family (vector extensions)
- 8MB PSRAM holds full IQ capture buffers for lossless Sub-GHz replay
- Controls SX1262 Sub-GHz transceivers (SPI)
- Controls NFC/RFID (AS3911B on PowerPack, PN532 on Spine)
- Continuous emergency service frequency monitoring (hardware GPIO kill to RP2350B)
- -1U variant: factory U.FL connector

### RP2350B — Master Controller & Safety Guardian
- Every Flipper command routes through RP2350B
- Safety checks on dedicated FreeRTOS task at 50ms intervals
- Flipper Zero UART bridge (UART0, GPIO 0/1)
- GPS NMEA parser (UART1, GPIO 8/9)
- OTA orchestrator — monitors C5 and S3 heartbeat GPIOs
- Audit logger — append-only TX event log to SD with GPS + timestamp
- Display driver (PowerPack status OLED)
- USB host controller — Flipper DFU flashing via permanent internal USB-C pigtail
- PowerPack detection via GPIO26 going HIGH at boot

---

## THREE BOARD ARCHITECTURE

### BOARD 1 — "The Spine" (65 × 35mm, 8mm thick)
Clips directly onto Flipper Zero GPIO header (top).
Runs indefinitely from Flipper 3.3V for all passive operations.
USB-C (our board) for active ops and RP2350B Neuro-Ducky.

Chips:    ESP32-C5-WROOM-1U + ESP32-S3-WROOM-1U-N16R8 + RP2350B
Radio:    1× SX1262 (E22-900M22S) Sub-GHz | PN532 HF NFC | NEO-M9N GNSS
Antennas: C5 U.FL (ext 2.4/5GHz dual-band dipole)
          S3 U.FL (ext 2.4GHz)
          SMA J3 (Sub-GHz 150–960MHz whip)
          NEO-M9N onboard ceramic patch (GPS)
PowerPack interface: 2× 2.54mm 16-pin pogo array (underside)
Pigtail:  100mm flat USB-C permanently routed inside case → Flipper USB-C

### BOARD 2 — "The PowerPack" (65 × 35mm, 10mm thick inc battery)
Slides under The Spine. Pogo pins mate with Spine underside pads.
Adds battery, power management, LF RFID, second Sub-GHz, extra antennas.

Chips:    BQ25895 (I2C battery + OTG) | HUSB238 (USB PD 3.0 sink)
          AS3911B (combined LF 125kHz + HF 13.56MHz RFID)
          2nd SX1262 (E22-900M22S) — dedicated TX path
Battery:  2500mAh 1S LiPo (flat cell, integrated in case)
Antennas: SMA J4 (Sub-GHz TX dedicated whip)
          U.FL J5 (2.4GHz second, spatial diversity)
          LF coil (125kHz, external, in case lid, JST-ZH connector)
USB-C:    USB PD 3.0 in (9V/2A) + Flipper charge OTG out (5V/1A)
Display:  0.91" SSD1306 128×32 OLED — battery/charge/ES status only

### BOARD 3 — "The Sandwich" (65 × 35mm, 16mm thick)
Spine + PowerPack integrated as single PCB, shared ground plane.
Sold as assembled unit. Pro Kit retail SKU.
Slightly thinner than 1+2 stacked (no pogo gap). Same functional footprint.

---

## PHYSICAL CONSTRAINTS

Flipper Zero body:       100 × 40 × 25mm
Our board width:         65mm (GPIO header span + SMA clearance)
Stack heights:
  Spine alone:           8mm above Flipper → total 33mm ✓
  Spine + PowerPack:    18mm above → total 43mm (manageable, two Flipper thicknesses)
  Sandwich:             15mm above → total 40mm (preferred form factor)
Width:                   65mm = Flipper width, no protrusion beyond case

---

## POWER ARCHITECTURE

### Spine (no battery)
  Passive ops (CSI, wardrive, GPS, NFC read):
    Flipper GPIO 3.3V sufficient. ~300mA draw. Flipper manages fine.

  Active TX peaks (~900mA):
    Plug Spine USB-C J1 → AP2112K-3.3 LDO → 3.3V_MAIN
    TPS2115A auto-selects: Flipper GPIO OR USB-C (whichever present)

### PowerPack — BQ25895 simultaneous power path
  Input (USB PD 9V/2A via HUSB238):
    → Powers system (buck path)
    → Charges LiPo (charger path)
    → Charges Flipper via OTG pigtail (boost path)
  All three simultaneously. No sequencing.

  Power budget (9V/2A = 18W):
    System worst case TX:    ~3.3W
    LiPo charge (1.5A):      ~6.3W
    Flipper charge (1A):     ~5.0W
    Efficiency losses (~85%): ~2.6W
    Total:                   ~17.2W — 0.8W headroom. Tight but valid.

  RP2350B dynamic load management (I2C to BQ25895):
    ACTIVE TX:  Flipper charge → 500mA, LiPo charge → 800mA
    IDLE/SCAN:  Flipper charge → 1A, LiPo charge → 1.5A (full)
    BATTERY ONLY: OTG 5V/800mA to Flipper until VBAT < 3.5V
    BELOW 3.5V: OTG disabled, system survives, user warned

  Field time:
    Passive scan:  ~11.5 hours
    Active TX:     ~4.2 hours
    From PD wall:  Unlimited + charges both simultaneously

### Permanent USB-C pigtail (always-connected to Flipper)
  100mm flat USB-C cable, soldered to our PCB, permanently plugs into Flipper USB-C.
  Routed in internal channel in right case face. User never touches it.
  Functions:
    NORMAL:   BQ25895 OTG charges Flipper continuously (5V/1A)
    FLASHING: RP2350B USB host mode, DFU flashing via STM32 ROM bootloader
  Clean separation: USB-C = power + flash only. UART GPIO = all data.

---

## POGO PIN INTERFACE (16 pins, 2.54mm pitch)

Pin  1-2:  GND
Pin  3-4:  3.3V_MAIN (PowerPack LDO → Spine when PowerPack present)
Pin  5:    I2C_SDA (RP2350B GPIO10 → BQ25895 + HUSB238)
Pin  6:    I2C_SCL (RP2350B GPIO11)
Pin  7:    SX1262_B_CS (S3 IO33 → second SX1262)
Pin  8-10: SX1262_B_CLK / MOSI / MISO (shared SPI bus, dedicated CS)
Pin  11:   SX1262_B_BUSY (→ S3 IO39)
Pin  12:   SX1262_B_DIO1 (→ S3 IO40)
Pin  13:   AS3911B_CS (→ S3 IO41)
Pin  14:   AS3911B SPI (shared bus, unique CS)
Pin  15:   POWERPACK_DETECT (pulled HIGH by PowerPack → RP2350B GPIO26)
Pin  16:   VBAT_SENSE (BQ25895 VBAT ÷ 2 → RP2350B ADC)

Notes:
  RP2350B detects PowerPack via GPIO26 HIGH on boot.
  If absent: PowerPack features gracefully disabled, no error.
  I2C pull-ups (4.7k) on Spine side. PowerPack I2C devices float when absent.
  AS3911B shares SPI bus with SX1262_B (different CS). Both on S3 SPI2.

---

## FIRMWARE ARCHITECTURE

### RP2350B FreeRTOS Task Map
  task_uart_bridge    — Flipper UART command receive/dispatch (priority 5)
  task_safety_guard   — Kill switch, ES monitor, whitelist enforcement (priority 6, 50ms)
  task_gps_parser     — NMEA parse from NEO-M9N (priority 3)
  task_audit_logger   — Append-only SD log of all TX events (priority 3)
  task_power_manager  — BQ25895 + HUSB238 I2C monitoring (priority 4)
  task_ota_monitor    — C5/S3 heartbeat GPIO watch (priority 2)
  task_display        — PowerPack OLED status refresh (priority 1)
  task_onboarding     — First-flash setup wizard (runs once, sets TEST_MODE flag)

### ESP32-C5 Task Map
  task_csi_collector    — Wi-Fi CSI capture at 20Hz, feeds fusion engine
  task_ble_scanner      — BLE advertisement density monitoring
  task_zigbee_monitor   — 802.15.4 passive channel activity
  task_ghost_fusion     — Multi-source RF environment fusion (see GHOST_TRACKER)
  task_wifi_active      — Scan, deauth, evil twin, beacon flood (active mode only)
  task_ota_client       — OTA manifest fetch + binary download
  task_emserv_monitor   — Emergency service frequency passive scan (always-on)

### ESP32-S3 Task Map
  task_tflite_inference — TFLite Micro model runner
  task_sx1262_primary   — Sub-GHz RX capture (SX1262_A)
  task_sx1262_secondary — Sub-GHz TX / dual-mode (SX1262_B, PowerPack only)
  task_nfc_rfid         — HF/LF card operations (AS3911B or PN532)
  task_fuzzy_engine     — RF anomaly detection + fuzzy noise generation
  task_rfp_engine       — RF device fingerprinting

---

## OTA FIRMWARE UPDATE FLOW

No programmer ever needed after initial flash:

  1. Flipper menu → System → Check for Updates
  2. C5 connects to saved Wi-Fi, fetches GitHub Releases manifest (JSON)
  3. SHA256 verified before download begins
  4. C5: dual-bank OTA write to inactive partition
  5. S3: updated via C5 UART relay bridge
  6. RP2350B: UF2 bootloader method (C5 writes .uf2 to SD, triggers UF2 boot)
     Previous RP2350B firmware preserved: /sd/ota/rp2350b_prev.uf2
  7. 60-second watchdog on all chips — auto-rollback if heartbeat lost
  8. Version confirmed via CDC read before user notified

  Flipper firmware flash (from our menu):
    RP2350B USB host → STM32 ROM DFU (VID 0x0483 PID 0xDF11)
    Chunk-level CRC verification. SHA256 pre-flight. Anti-brick guarantees.
    DFU ROM is hardware — cannot be overwritten. Recovery always possible.

---

## TEST MODE

Set at first flash via onboarding wizard (runs once, stored in NVS).
Not a runtime toggle — requires intentional re-flash or NVS clear to change.

TEST MODE enables:
  - All range/distance limits default to minimum safe values
  - TX power locked to 10mW maximum until manually overridden per-session
  - All ML inference outputs displayed with confidence scores and raw data
  - Extended audit logging (passive captures also logged, not just TX events)
  - Checklist UI presented before each capability category (see BENCH_TEST_ALPHA.md)
  - Ducky scripts require TEST_MODE=true to execute (additional friction layer)
  - fuzzy_noise engine shows live anomaly scores and signal delta visualisation
  - All EMSERV detections logged verbosely regardless of TX state

TEST MODE flag visible in:
  - Flipper status bar: [TEST] indicator when active
  - Every audit log entry tagged TEST_MODE:true
  - Version string: "3.2.0-TEST" vs "3.2.0"

---

## DUCKY SCRIPT / BADUSB ARCHITECTURE

RP2350B presents as USB HID keyboard via USB-C. Rubber Ducky-class capability.
Scripts loaded from /sd/ducky/ directory. Triggered from Flipper menu.

### Whitelisted command categories (default TEST_MODE and normal mode)
  CATEGORY_INFO       — System enumeration: whoami, hostname, ifconfig, ipconfig
  CATEGORY_NETWORK    — Network diagnostics: ping, tracert, nslookup, arp
  CATEGORY_FILE_READ  — Read-only file operations: dir, ls, cat, type
  CATEGORY_AUDIT      — Security audit tools: netstat, tasklist, ps, sc query

### Restricted categories (require override flag in script header)
  CATEGORY_EXECUTE    — Run executables or scripts
  CATEGORY_MODIFY     — Write, delete, or modify files
  CATEGORY_PERSIST    — Registry, cron, startup modifications
  CATEGORY_EXFIL      — Data transfer or network connections

### Override flag syntax
  # NEURO_OVERRIDE: CATEGORY_EXECUTE
  # REASON: Authorised red team engagement — ClientCo pentest 2025-01-15
  # OPERATOR: [name]
  # This header is logged to audit before execution begins.

### Professional script templates provided
  /sd/ducky/templates/enum_windows.nd     — Full Windows enumeration
  /sd/ducky/templates/enum_linux.nd       — Full Linux enumeration
  /sd/ducky/templates/net_audit.nd        — Network security audit
  /sd/ducky/templates/cred_check.nd       — Credential store audit (read-only)
  Custom scripts: .nd extension, UTF-8, Ducky Script 2.0 syntax + our extensions

### Legal warning
  Every Ducky execution (template or custom) triggers:
  "BadUSB execution requires authorisation. Target device must be owned by you
   or covered by written engagement permission. Continue? [YES/NO]"
  Response logged to audit with timestamp + GPS.

---

## RF VECTORS LIBRARY

Bundled identification signatures + user import + community GitHub contributions.

### What we bundle (identification only, not exploitation)
  /sd/rf_vectors/base/
    subghz_protocols/    — OOK/FSK signature library for common protocols
                           (keyfobs, building automation, ISM devices)
    ble_signatures/      — Known BLE device advertisement fingerprints
    wifi_signatures/     — Known management frame patterns
    medical_protocols/   — Published medical device RF protocol identifiers
                           (WMTS 608MHz, MedRadio 401–406MHz, ISM telemetry)

### User import format
  /sd/rf_vectors/user/   — Drop .nrfv files (Neuro RF Vector format, JSON-based)
  Format spec:           /sd/rf_vectors/FORMAT_SPEC.md

### Community contributions
  GitHub: github.com/robotard/neuro-sniffer-pro/tree/main/rf_vectors/community
  PR process documented in CONTRIBUTING.md
  Review required before merge — identification signatures only, no weaponised payloads
  CC-BY-NC-SA licence (same as ML models)

### Curation principle
  Vectors teach the tool to RECOGNISE. Professionals decide what to DO.
  No bundled exploitation payloads. No rolling code attack sequences.
  The distinction: "this looks like a HID Prox card" vs "here's how to clone it silently".
  The former is ours. The latter is the professional's own tooling.

---

## PRODUCT SKUs AND PRICING

| SKU | Description | Price (GBP) | COGS (USD) | Margin |
|-----|-------------|-------------|------------|--------|
| Spine | Board 1, assembled | £99.99 | ~$52 | ~55% |
| PowerPack | Board 2, assembled | £79.99 | ~$45 | — |
| Sandwich | Integrated, assembled | £159.99 | ~$80 | ~55% |
| Spine + Flipper bundle | Entry kit | £379.99 | ~$297 | ~30% |
| DIY Spine (5× bare PCBs) | Maker tier | £34.99 | ~$12 | — |
| DIY Full kit (5× Spine + 5× PowerPack) | Builder tier | £54.99 | ~$20 | — |
| Enterprise Red Team licence | Sandwich + 12mo updates + HF model access | £699.00 | ~$80 | ~86% |

---

## COMPETITIVE POSITION

| Product | Price | Battery | 5GHz | LF RFID | Dual Sub-GHz | PD | AI | AR-ready |
|---------|-------|---------|------|---------|-------------|-----|-----|---------|
| Flipper Wi-Fi module | $29 | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ |
| HackRF One | $340 | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ |
| Proxmark3 RDV4 | $370 | ✗ | ✗ | ✓ | ✗ | ✗ | ✗ | ✗ |
| WiFi Pineapple Nano | $99 | ✗ | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |
| **Neuro-Sniffer Spine** | **£99** | **✗** | **✓** | **✗** | **✗** | **✓** | **✓** | **✓** |
| **Neuro-Sniffer Sandwich** | **£159** | **✓** | **✓** | **✓** | **✓** | **✓** | **✓** | **✓** |

---

*v3.2 | robotard | Robustas Design & Engineering | github.com/robotard/neuro-sniffer-pro*
