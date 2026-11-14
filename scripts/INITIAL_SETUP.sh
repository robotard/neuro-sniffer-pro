#!/usr/bin/env bash
# INITIAL_SETUP.sh — Neuro-Sniffer Pro v0.3.3.0
# Installs ESP-IDF v5.5.2+ (C5 support) and Pico SDK 2.0+ (RP2350B support).
# Toolchain only — does NOT flash anything. Use FLASH_TEST.sh or
# FLASH_PRODUCTION.sh for that; both go through the safety checklist.
set -euo pipefail

echo "=== Neuro-Sniffer Pro v0.3.3.0 — Toolchain Setup ==="
echo "Chips: ESP32-C5-WROOM-1U | ESP32-S3-WROOM-1U-N16R8 | RP2350B"
echo ""

IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf}"
PICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/pico/pico-sdk}"

if [ ! -d "$IDF_PATH" ]; then
    echo "[ESP-IDF] Cloning v5.5.2 (C5 requires >= v5.5)..."
    mkdir -p "$(dirname "$IDF_PATH")"
    git clone --recursive --branch v5.5.2 \
        https://github.com/espressif/esp-idf.git "$IDF_PATH" -q
    (cd "$IDF_PATH" && ./install.sh esp32c5,esp32s3)
    echo "export IDF_PATH=\"$IDF_PATH\"" >> ~/.bashrc
    echo 'alias get_idf=". $IDF_PATH/export.sh"' >> ~/.bashrc
    echo "[WARN] IDFGH-17558 (5GHz CSI bug) present in v5.5.2 — see docs/FIRMWARE_NOTES.md"
else
    echo "[OK] ESP-IDF found: $IDF_PATH"
fi

if [ ! -d "$PICO_SDK_PATH" ]; then
    echo "[Pico SDK] Cloning v2.1.0 (RP2350B requires SDK >= 2.0)..."
    mkdir -p "$(dirname "$PICO_SDK_PATH")"
    git clone --recursive --branch 2.1.0 \
        https://github.com/raspberrypi/pico-sdk.git "$PICO_SDK_PATH" -q
    echo "export PICO_SDK_PATH=\"$PICO_SDK_PATH\"" >> ~/.bashrc
else
    echo "[OK] Pico SDK found: $PICO_SDK_PATH"
fi

echo ""
echo "[OK] Toolchain ready."
echo "Next: bash scripts/FLASH_TEST.sh"
