# SETUP_GUIDE.md — Neuro-Sniffer Pro v0.3.3.0

End-user first-boot guide for an **assembled** unit. If you're building your
own board from bare PCB/kit, see `docs/DIY_BUILD_GUIDE.md` instead — this
guide assumes the hardware already works and just needs first-time setup.
If you're a firmware developer flashing from source, see
`docs/FIRMWARE_NOTES.md` instead, which covers the dev flash procedure.

## 1. Before you power on

- You should have received the device with the kill switch in the OFF
  position. Leave it off until step 4.
- You'll need a Flipper Zero running a current firmware, and a USB-C cable.

## 2. Physical setup

1. Clip the Spine (or Sandwich) board onto your Flipper Zero's GPIO header.
2. If you have the PowerPack board, confirm it's seated and the LiPo is
   connected before first boot — the PowerPack supplies USB PD 3.0 charging
   via the HUSB238 and runs the power path through the BQ25895.
3. Leave the kill switch OFF for the first boot.

## 3. First boot — the onboarding wizard

The very first flash sets **TEST_MODE**, which fixes your TX power cap at
either 10mW (test) or 100mW (production) for the life of that flash — this
is **not** a setting you can change later at runtime; it's deliberately a
first-flash-only choice (see ADR-004). If your unit arrived pre-flashed from
the factory, this was already set during manufacturing
(`scripts/FLASH_PRODUCTION.sh`) and you won't see this wizard.

If you're setting up a dev/bench unit yourself:
1. Connect via USB-C.
2. Run the onboarding flow through the Flipper app (menu path TBD until the
   Flipper Lua app's onboarding screen is finalised — see
   `software/flipper_lua/rf_neuro.lua`).
3. Confirm your TEST_MODE choice. Test mode (10mW) is strongly recommended
   for anyone who isn't doing authorised client work with a known RF range
   requirement.

## 4. Turning on the kill switch

Only after onboarding completes: flip the physical kill switch ON. This
powers the RF VCC rail — nothing transmits before this point, by hardware
design (cut wire or switch left off = RF off, not on).

## 5. Understanding the safety notifications

This device does **not** silently block emergency-service or aviation
bands. Per ADR-003, it makes you — the operator — aware when it detects
proximity to those bands during passive monitoring, so you don't
accidentally run an active test near them. You'll see this as an
`ALERT:EMERGENCY_SHIELD_ACTIVE` notification on the Flipper UI. Active
transmission that overlaps a detected emergency-service band is blocked by
default; overriding that block (for an authorised professional with a
legitimate reason) requires the separate, deliberately-hard-to-reach
"Advanced Security Professional" mode described in the project brief —
**this override mode is not implemented in the current firmware yet**, so
right now an ES-overlap block cannot be overridden at all. That's a
limitation to be aware of, not a feature being hidden from you.

## 6. Checking device status anytime

From the Flipper menu (or directly over UART at 115200 baud), send
`SAFETY_STATUS` to get the current kill-switch state, ES-alert state,
RF-VCC state, TX power cap, and last block reason as a JSON line.

## 7. If something doesn't respond

- No response at all: check the kill switch position and that the unit is
  getting power (USB-C or PowerPack battery).
- `ERROR:SAFETY_LOCKED` on an active-TX command: check `SAFETY_STATUS` —
  most commonly the kill switch is off, or an ES alert is currently active.
- For anything else, this is pre-alpha hardware — there is no public support
  channel yet. File an issue against
  `github.com/robotard/neuro-sniffer-pro`.

## References
- `docs/decisions/ADR-003-es-monitor-notify-not-block.md`
- `docs/decisions/ADR-004-test-mode-first-flash.md`
- `docs/LEGAL_DISCLAIMER.md`
- `docs/DIY_BUILD_GUIDE.md` (for self-assembled units)
- `docs/FIRMWARE_NOTES.md` (for dev/source flashing)
