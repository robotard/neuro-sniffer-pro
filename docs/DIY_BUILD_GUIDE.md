# DIY_BUILD_GUIDE.md — Neuro-Sniffer Pro v0.3.3.0

For bare-PCB / kit buyers assembling their own Spine board rather than buying
an assembled unit. This guide assumes basic SMT soldering experience
(0402 passives, QFN packages) or access to a reflow oven / hot-air station.
**Pre-alpha notice:** no alpha board has been bench-verified yet (see project
brief). This guide describes the intended build process; steps will be
corrected against the real board once one exists — treat anything here as
provisional until FLASH_TEST.sh has been run successfully on a real unit.

## 1. What you need

**From this repo:**
- Gerbers / KiCad project for the Spine board (once published — see
  `hardware/PCBWAY_KICAD_AI_PROMPT_SPINE.md` for the design brief used to
  generate them).
- `hardware/BOM.csv` — full parts list with manufacturer part numbers.
- `scripts/INITIAL_SETUP.sh`, `scripts/FLASH_TEST.sh`.

**Tools:**
- Soldering iron with fine tip (for connectors/headers), or hot-air/reflow
  for the QFN parts (RP2350B, PN532) and the ESP32 modules.
- Multimeter (continuity + voltage checks before first power-on — do this,
  don't skip it).
- USB-C cable, a computer with the ESP-IDF v5.5.2+ and Pico SDK 2.0+
  toolchains (installed via `scripts/INITIAL_SETUP.sh`).
- A Flipper Zero, for final GPIO-header fit and bridge testing.
- Antistatic precautions — the RF front-end parts are not cheap to re-source
  individually.

## 2. Assembly order

1. **Passives first** (C1–30, C26–30, L1, R1–10) — lowest profile, least
   thermal risk, easiest to inspect before anything taller is in the way.
2. **QFN/fine-pitch ICs** (RP2350B U3, PN532 U5, TPS2115A U7, MT3608 U8) —
   reflow or careful hot-air. Check for solder bridges under magnification;
   QFN packages hide bridged pins.
3. **ESP32 modules** (ESP32-C5-WROOM-1 U1, ESP32-S3-WROOM-1 U2) — these are
   pre-shielded castellated modules; standard reflow profile for the module,
   not the bare die.
4. **Sub-GHz module** (E22-900M22S U4) and **GNSS module** (NEO-M9N U6).
5. **Connectors last** (USB-C J1, Flipper header J2, SMA J3, OLED header J4,
   microSD J5, kill switch SW1) — these take the most mechanical stress
   during use, so solder them with full wetting and inspect for cold joints.
6. **RF shield cans** (Shield1 over C5, Shield2 over S3) — fit only after
   you've confirmed both chips boot and respond without the can on. Do not
   seat the cans until bench test passes — you will want rework access.

## 3. Before first power-on

- Visual inspection under magnification for solder bridges, especially on
  the two QFN parts and the ESP32 module castellations.
- Continuity check: no short between 3V3 rail and GND anywhere on the board.
- Confirm the kill switch (SW1) is in the OFF position before connecting
  power for the very first time — RF VCC should read 0V with the switch off,
  per the fail-safe design (cut wire or disengaged switch = RF off, not RF on).

## 4. First boot

1. Connect via USB-C. Do **not** connect to a Flipper Zero yet.
2. Run `bash scripts/FLASH_TEST.sh` — this is the alpha bench-flash path,
   gated behind the TEST_MODE safety checklist (10mW TX power cap, not
   100mW). It will refuse to proceed if you skip the checklist.
3. Watch for `NEURO:READY` on the UART console (115200 baud) — this comes
   from `firmware/rp2350b_master/src/main.cpp`.
4. Send `SAFETY_STATUS` over UART and confirm you get a `SAFETY:{...}` JSON
   reply with `"kill":0` (kill switch read as disengaged, since SW1 is off).
   If you don't get a reply, stop and recheck soldering before going further —
   don't proceed to flashing C5/S3 until the RP2350B safety path is confirmed
   alive.
5. Only once that's confirmed: flip the kill switch on, confirm `"kill":1`,
   then proceed to flashing the ESP32-C5 and ESP32-S3 firmware per
   `docs/FIRMWARE_NOTES.md`.

## 5. Common first-build issues

- **No UART output at all**: check the USB-C data lines are actually wired
  (some USB-C receptacles are power-only); check RP2350B is getting 3V3.
- **Board draws power but nothing boots**: reflow profile too hot/cold on
  the RP2350B QFN56 — this is the most common cold-joint location on a
  hand-built unit.
- **C5/S3 don't respond to SPI commands**: check CS line polarity
  (active-low) and that the shield cans weren't fitted before confirming
  both modules boot standalone.

## 6. Final fit

Clip the assembled Spine board onto the Flipper Zero's GPIO header (J2) only
after all of the above passes. Run the full alpha checklist in
`docs/TEST_PLAN.md` before treating the unit as usable for any real testing
work.

## References
- `hardware/BOM.csv`, `hardware/PCBWAY_KICAD_AI_PROMPT_SPINE.md`
- `scripts/INITIAL_SETUP.sh`, `scripts/FLASH_TEST.sh`
- `docs/FIRMWARE_NOTES.md`, `docs/TEST_PLAN.md`
- `docs/decisions/ADR-004-test-mode-first-flash.md`
