#!/usr/bin/env bash
# BUILD_REPO.sh — Neuro-Sniffer Pro v0.3.3.0 — Linux
#
# Git is the single source of truth for this project. This script does
# NOT embed any file content — it clones the real repo and sets up the
# local toolchain.
#
# What this does:
#   1. Clone github.com/robotard/neuro-sniffer-pro (or pull if it exists)
#   2. Install ESP-IDF v5.5.2+ (ESP32-C5/S3 support)
#   3. Install Pico SDK 2.0+ (RP2350B support)
#
# Usage: bash BUILD_REPO.sh [target_dir]
set -euo pipefail

REPO_URL="https://github.com/robotard/neuro-sniffer-pro.git"
TARGET_DIR="${1:-neuro-sniffer-pro}"

echo "=== Neuro-Sniffer Pro v0.3.3.0 — Repo + Toolchain Setup ==="

if [ -d "$TARGET_DIR/.git" ]; then
    echo "[git] $TARGET_DIR exists — pulling latest..."
    (cd "$TARGET_DIR" && git pull)
else
    echo "[git] Cloning $REPO_URL..."
    git clone "$REPO_URL" "$TARGET_DIR"
fi

cd "$TARGET_DIR"

echo ""
echo "[toolchain] Delegating to scripts/INITIAL_SETUP.sh..."
bash scripts/INITIAL_SETUP.sh

echo ""
echo "=== Done ==="
echo "cd $TARGET_DIR && bash scripts/FLASH_TEST.sh   (alpha bench flash)"