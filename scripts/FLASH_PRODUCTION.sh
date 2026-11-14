#!/usr/bin/env bash
# FLASH_PRODUCTION.sh — Neuro-Sniffer Pro v0.3.3.0
# Production flash — TEST_MODE=false, 100mW TX cap available
set -euo pipefail
RED='\033[0;31m'; GREEN='\033[0;32m'; NC='\033[0m'; BLUE='\033[0;34m'
echo -e "${RED}"
echo "╔══════════════════════════════════════════════════╗"
echo "║   NEURO-SNIFFER PRO v0.3.3.0 — PRODUCTION       ║"
echo "║   TEST_MODE=false | TX_CAP=100mW                 ║"
echo "╚══════════════════════════════════════════════════╝"
echo -e "${NC}"
echo -e "${RED}PRODUCTION CHECKLIST:${NC}"
echo "  [ ] End-user vetted and qualified professional"
echo "  [ ] Legal disclaimer accepted and on file"
echo "  [ ] Serial number assigned"
read -rp "All confirmed? [type PRODUCTION]: " confirm
if [[ "$confirm" != "PRODUCTION" ]]; then echo "Aborted."; exit 1; fi
read -rp "Serial number for this unit (e.g. NSP-001): " SERIAL
export TEST_MODE=false; export TX_POWER_CAP_MW=100
export NSP_SERIAL="$SERIAL"; export GHOST_BAND=WIFI_BAND_2G
source "${IDF_PATH:-$HOME/esp/esp-idf}/export.sh" 2>/dev/null || true
echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) PRODUCTION serial=$SERIAL" >> production_flash_log.txt

echo -e "\n${BLUE}[1/3] RP2350B...${NC}"
read -rp "BOOTSEL + connect. ENTER when mounted: " _
[[ "$(uname)" == "Darwin" ]] && MOUNT="/Volumes/RPI-RP2" || MOUNT="/media/$USER/RPI-RP2"
cd firmware/rp2350b_master
cmake -B build -DTEST_MODE=0 -DTX_POWER_CAP_MW=100 -DPICO_BOARD=pico2 -DNSP_SERIAL="$SERIAL" .
cmake --build build --parallel
cp build/neuro_rp2350b.uf2 "$MOUNT/"; sleep 3; cd ../..
echo -e "${GREEN}[OK] RP2350B${NC}"

echo -e "\n${BLUE}[2/3] ESP32-C5...${NC}"
read -rp "Port: " C5_PORT
cd firmware/esp32_c5_rf
idf.py set-target esp32c5 2>/dev/null || true
idf.py build -DTEST_MODE=0 -DGHOST_BAND=WIFI_BAND_2G
idf.py -p "$C5_PORT" flash; cd ../..
echo -e "${GREEN}[OK] C5${NC}"

echo -e "\n${BLUE}[3/3] ESP32-S3...${NC}"
read -rp "Port: " S3_PORT
cd firmware/esp32_s3_ai
idf.py set-target esp32s3 2>/dev/null || true
idf.py build -DTEST_MODE=0; idf.py -p "$S3_PORT" flash; cd ../..
echo -e "${GREEN}PRODUCTION FLASH COMPLETE: $SERIAL${NC}"
echo "Logged to production_flash_log.txt"
