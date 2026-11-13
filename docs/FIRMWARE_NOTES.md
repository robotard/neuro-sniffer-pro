# Firmware Notes — Neuro-Sniffer Pro v0.3.3.0
# ESP-IDF version requirements, known bugs, and patch paths
# github.com/robotard/neuro-sniffer-pro

---

## ESP-IDF VERSION REQUIREMENTS

| Chip | Minimum IDF | Recommended | Notes |
|------|-------------|-------------|-------|
| ESP32-C5 | v5.5.0 | v5.5.2+ with patch | C5 production support starts v5.5 |
| ESP32-S3 | v5.1.0 | v5.5.2+ | Align with C5 for single IDF install |
| RP2350B | Pico SDK 2.0+ | 2.1+ | SDK 2.0 required for RP2350B support |

---

## KNOWN BUG: IDFGH-17558 — 5GHz CSI STATIC BUFFERS

### Summary
On ESP32-C5 under ESP-IDF v5.5.4 and earlier, 5GHz Wi-Fi CSI buffers
are allocated statically and do not update after initial allocation.
2.4GHz CSI works correctly. Ghost Tracker on 5GHz returns frozen data.

### Impact
- Ghost Tracker on 5GHz: BROKEN (static CSI, no motion data)
- Ghost Tracker on 2.4GHz: WORKING CORRECTLY
- RF Fingerprinting on 5GHz: PARTIALLY AFFECTED (CFO still readable, CSI flatness frozen)
- fuzzy_noise on 5GHz: AFFECTED (anomaly engine needs live CSI variance)
- All other 5GHz functions (scan, deauth, evil twin, PMKID): UNAFFECTED

### Fix Path A — Pinned IDF Commit (Recommended for Alpha)
A fix was submitted in the IDF tracker. Pin to the commit immediately
after the confirmed fix lands:

```bash
cd ~/esp/esp-idf
git fetch origin
git checkout <CONFIRMED_FIX_COMMIT>   # Update this when fix is merged
git submodule update --init --recursive
./install.sh esp32c5,esp32s3
```

Check issue status at:
https://github.com/espressif/esp-idf/issues/17558

### Fix Path B — Manual Patch (If Fix Not Yet Merged)
Apply the patch manually to the CSI buffer allocation in the Wi-Fi driver:

File to patch:
```
~/esp/esp-idf/components/esp_wifi/src/wifi_init.c
```

Locate the CSI buffer allocation block (search for `wifi_csi_config`).
Change static allocation to dynamic allocation per the fix discussion
on the GitHub issue thread. The patch is straightforward once you have
the confirmed fix diff from the issue.

### Fix Path C — Workaround for Alpha (No Patch Required)
Operate Ghost Tracker on 2.4GHz only for alpha testing:

In `firmware/esp32_c5_rf/src/ghost_tracker.c`, the channel config
already has a `GHOST_BAND` compile flag:

```c
#define GHOST_BAND  WIFI_BAND_2G   // Use until IDFGH-17558 resolved
// #define GHOST_BAND  WIFI_BAND_5G   // Enable after patch confirmed
// #define GHOST_BAND  WIFI_BAND_BOTH // Enable after patch confirmed
```

`FLASH_TEST.sh` sets `GHOST_BAND=WIFI_BAND_2G` by default.
Alpha bench testing proceeds normally on 2.4GHz.
5GHz Ghost Tracker deferred to post-alpha.

### Verification Test
After applying any fix:
```bash
# Run CSI buffer validation test
idf.py -p $PORT flash && idf.py -p $PORT monitor | grep "CSI_5G_VALID"
# Expected output: CSI_5G_VALID: YES — buffer updating at 20Hz
# Bug output:      CSI_5G_VALID: NO  — buffer frozen
```

---

## INITIAL FLASH PROCEDURE — ALL THREE CHIPS

### Prerequisites
```bash
# macOS
brew install cmake ninja python3
source ~/esp/esp-idf/export.sh   # or: get_idf

# Linux
source ~/esp/esp-idf/export.sh
```

### Step 1 — RP2350B (UF2 method)
```bash
# Hold BOOTSEL on RP2350B and connect USB-C
# Drive mounts as RPI-RP2

# macOS
cp firmware/rp2350b_master/build/neuro_rp2350b.uf2 /Volumes/RPI-RP2/

# Linux
cp firmware/rp2350b_master/build/neuro_rp2350b.uf2 /media/$USER/RPI-RP2/

# RP2350B reboots automatically after copy. Done.
```

### Step 2 — ESP32-C5 (esptool via ESP-IDF)
```bash
cd firmware/esp32_c5_rf

# Set target (once per checkout)
idf.py set-target esp32c5

# Configure (set Wi-Fi credentials, OTA server URL, TEST_MODE flag)
idf.py menuconfig
# → Neuro-Sniffer Config → Wi-Fi SSID/Password
# → Neuro-Sniffer Config → OTA Server URL
# → Neuro-Sniffer Config → TEST_MODE (true for alpha, false for production)

# Build and flash
idf.py -p /dev/cu.usbserial-XXXXX build flash monitor   # macOS
idf.py -p /dev/ttyUSB0 build flash monitor              # Linux

cd ../..
```

### Step 3 — ESP32-S3 (esptool via ESP-IDF)
```bash
cd firmware/esp32_s3_ai
idf.py set-target esp32s3

# Build and flash (inherits TEST_MODE from shared sdkconfig)
idf.py -p /dev/cu.usbserial-YYYYY build flash monitor   # macOS
idf.py -p /dev/ttyUSB1 build flash monitor              # Linux

cd ../..
```

### Step 4 — Onboarding Wizard
On first boot after flash, the RP2350B runs the onboarding wizard via Flipper:
```
Neuro-Sniffer Pro v0.3.3.0
First Boot Setup
[1/4] Confirm jurisdiction: [YOUR COUNTRY]
[2/4] Confirm authorisation: I have written permission for all targets [CONFIRM]
[3/4] Test Mode: [ENABLED / DISABLED]
[4/4] Wi-Fi credentials: [SSID] [PASSWORD]
Setup complete. TEST_MODE stored in NVS.
```

TEST_MODE cannot be changed at runtime. Re-flash or NVS clear required.

---

## OTA PARTITION TABLE

```
# ESP32-C5 partition table
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x6000,
otadata,  data, ota,     0xf000,   0x2000,
phy_init, data, phy,     0x11000,  0x1000,
ota_0,    app,  ota_0,   0x20000,  0x1C0000,
ota_1,    app,  ota_1,   0x1E0000, 0x1C0000,
storage,  data, spiffs,  0x3A0000, 0x60000,
```

Dual-bank OTA: ota_0 and ota_1 alternate. 60-second watchdog.
Auto-rollback to previous partition if heartbeat lost after boot.

---

## COMPILE FLAGS REFERENCE

| Flag | Values | Default | Notes |
|------|--------|---------|-------|
| GHOST_BAND | WIFI_BAND_2G, WIFI_BAND_5G, WIFI_BAND_BOTH | WIFI_BAND_2G | 5G pending IDFGH-17558 fix |
| TEST_MODE | true, false | true (FLASH_TEST.sh) | Set via onboarding or sdkconfig |
| TX_POWER_CAP | 10, 50, 100 (mW) | 10 (TEST), 100 (PROD) | RP2350B enforced |
| AUDIT_VERBOSE | true, false | true (TEST), false (PROD) | Passive events also logged in TEST |
| FUZZY_DEBUG | true, false | true (TEST), false (PROD) | Shows raw anomaly scores |
| GHOST_DEBUG | true, false | true (TEST), false (PROD) | Shows subcarrier variance heatmap |

---

## UART PROTOCOL (Flipper ↔ RP2350B)

Baud: 115200, 8N1, no flow control
Format: Newline-terminated JSON
Max command length: 512 bytes

```json
// Command (Flipper → RP2350B)
{"cmd":"WIFI_SCAN","params":{"band":"both","duration_ms":5000}}

// Response (RP2350B → Flipper)
{"cmd":"WIFI_SCAN","status":"ok","data":{...},"test_mode":true}

// Error
{"cmd":"WIFI_SCAN","status":"error","reason":"TX_WHITELIST_BLOCK","test_mode":true}
```

---

*v0.3.3.0 | robotard | github.com/robotard/neuro-sniffer-pro*
