# SD Card Schema — Neuro-Sniffer Pro v0.3.3.0
Format: FAT32, 32GB+ Class 10, 32KB allocation units.

## Directory Structure
    /sd/
    ├── ghost/csi/          # Binary CSI tensor logs (128B/frame @ 20Hz ≈ 9MB/hr)
    ├── ghost/rfp/          # RF fingerprint TSV (MAC, CFO, skew, flatness, GPS)
    ├── ghost/gps/          # GPS coordinate logs per CSI frame
    ├── captures/wifi/      # GPS-tagged wardrive PCAP files
    ├── captures/ble/       # BLE capture PCAPs
    ├── captures/subghz/    # Raw IQ binary + JSON metadata
    ├── captures/nfc/       # Card dump binary files
    ├── ml/models/          # Active TFLite models (deployed)
    ├── ml/training/        # IQ samples, labelled CSI, labelled RFP data
    ├── ml/staging/         # Downloaded models pending SHA256 verification
    ├── ml/zoo_index.json   # Local cache of HuggingFace model zoo manifest
    ├── ota/                # OTA firmware binaries + SHA256 + manifest
    ├── cache/              # Cross-SoC live state (JSON, updated every 500ms)
    │   ├── ghost_state.json    # motion/bpm/people/devices/frames/gps
    │   ├── scan_results.json   # Latest Wi-Fi scan output
    │   ├── gps_last.json       # Last GPS fix
    │   └── safety_state.json   # Kill/emergency/tx/power state
    ├── gui/fonts/          # RP2350B OLED bitmap fonts
    ├── gui/ducky/          # Neuro-Ducky BadUSB payload scripts
    └── audit/audit_YYYY.log # Append-only TX event log: timestamp+GPS+cmd

## Key File Formats

ghost_state.json (written by C5 via RP2350B every 500ms):
  {"motion":0.73,"bpm":14.2,"walk":true,"people":3,"devices":12,"frames":4400,"lat":51.5074,"lon":-0.1278}

ota_manifest.json (fetched from GitHub Releases API):
  {"c5_version":"2.1.0","c5_url":"https://...","c5_sha256":"abc...","s3_version":"2.1.0","s3_url":"https://...","s3_sha256":"def..."}

zoo_index.json (synced from HuggingFace):
  {"models":[{"name":"rf_classifier","version":"1.0.0","url":"https://huggingface.co/...","sha256":"...","size_kb":178,"accuracy":0.87}]}

## Health Monitoring
- Warn at <1GB free, halt OTA at <100MB free
- CSI logs rotate at 1GB or 24h
- Audit log is append-only, archived yearly, never deleted
