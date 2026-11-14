#!/usr/bin/env bash
# FLASH_TEST.sh — Neuro-Sniffer Pro v0.3.3.0
# Alpha bench flash — TEST_MODE=true, 10mW TX cap
# Chips: ESP32-C5-WROOM-1U + ESP32-S3-WROOM-1U-N16R8 + RP2350B
set -euo pipefail
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
echo -e "${YELLOW}"
echo "╔══════════════════════════════════════════════════╗"
echo "║   NEURO-SNIFFER PRO v0.3.3.0 — ALPHA FLASH      ║"
echo "║   TEST_MODE=true | TX_CAP=10mW | GHOST=2.4GHz   ║"
echo "╚══════════════════════════════════════════════════╝"
echo -e "${NC}"
echo -e "${RED}PRE-FLASH CHECKLIST:${NC}"
echo "  [ ] RF-shielded environment confirmed for any active TX tests"
echo "  [ ] Hardware kill switch accessible and functional"
echo "  [ ] Written permission for all TX targets on file"
read -rp "All confirmed? [type YES]: " confirm
if [[ "$confirm" != "YES" ]]; then echo "Aborted."; exit 1; fi

export TEST_MODE=true
export TX_POWER_CAP_MW=10
export GHOST_BAND=WIFI_BAND_2G
export AUDIT_VERBOSE=true
export FUZZY_DEBUG=true
source "${IDF_PATH:-$HOME/esp/esp-idf}/export.sh" 2>/dev/null || true

# RP2350B
echo -e "\n${BLUE}[1/3] Flashing RP2350B...${NC}"
read -rp "Hold BOOTSEL and connect USB-C. Press ENTER when RPI-RP2 mounted: " _
[[ "$(uname)" == "Darwin" ]] && MOUNT="/Volumes/RPI-RP2" || MOUNT="/media/$USER/RPI-RP2"
cd firmware/rp2350b_master
cmake -B build -DTEST_MODE=1 -DTX_POWER_CAP_MW=10 -DPICO_BOARD=pico2 .
cmake --build build --parallel
cp build/neuro_rp2350b.uf2 "$MOUNT/"
sleep 3; cd ../..
echo -e "${GREEN}[OK] RP2350B flashed${NC}"

# ESP32-C5
echo -e "\n${BLUE}[2/3] Flashing ESP32-C5...${NC}"
read -rp "Connect C5. Enter serial port: " C5_PORT
cd firmware/esp32_c5_rf
idf.py set-target esp32c5 2>/dev/null || true
idf.py build -DTEST_MODE=1 -DGHOST_BAND=WIFI_BAND_2G -DAUDIT_VERBOSE=1
idf.py -p "$C5_PORT" flash
cd ../..
echo -e "${GREEN}[OK] ESP32-C5 flashed${NC}"

# ESP32-S3
echo -e "\n${BLUE}[3/3] Flashing ESP32-S3...${NC}"
read -rp "Connect S3. Enter serial port: " S3_PORT
cd firmware/esp32_s3_ai
idf.py set-target esp32s3 2>/dev/null || true
idf.py build -DTEST_MODE=1 -DFUZZY_DEBUG=1
idf.py -p "$S3_PORT" flash
cd ../..
echo -e "${GREEN}[OK] ESP32-S3 flashed${NC}"

echo -e "\n${GREEN}ALPHA FLASH COMPLETE — TEST_MODE=true${NC}"
echo "Next: Connect Spine to Flipper Zero GPIO header"
echo "      Complete onboarding wizard on Flipper screen"
echo "      Confirm [TEST] in Flipper status bar"
