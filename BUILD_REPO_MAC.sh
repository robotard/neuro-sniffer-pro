#!/usr/bin/env bash
# BUILD_REPO_MAC.sh — Neuro-Sniffer Pro v0.3.3.0 — macOS
# Same model as BUILD_REPO.sh: git is the source of truth, this script
# only installs macOS prerequisites then clones + sets up toolchain.
set -euo pipefail

echo "=== Neuro-Sniffer Pro v0.3.3.0 — macOS Setup ==="

command -v brew &>/dev/null || \
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
eval "$(brew shellenv 2>/dev/null)" || true

brew install git cmake ninja python3 wget dfu-util 2>/dev/null || true
command -v gh &>/dev/null || brew install gh 2>/dev/null || true
command -v picotool &>/dev/null || brew install picotool 2>/dev/null || true

echo "[OK] macOS prerequisites ready."
echo ""

bash "$(dirname "$0")/BUILD_REPO.sh" "${1:-neuro-sniffer-pro}"