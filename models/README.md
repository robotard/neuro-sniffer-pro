# Model Zoo — Neuro-Sniffer Pro v0.3.3.0
Hub: https://huggingface.co/neuro-sniffer-pro | Licence: CC-BY-NC-SA

| Model | Task | Size | Accuracy |
|---|---|---|---|
| rf_classifier.tflite | 6-class signal ID | ~180KB | ~87% |
| ghost_motion.tflite | Walk/breathe/static | ~95KB | ~91% |
| crowd_count.tflite | Occupancy estimate | ~120KB | ~79% |
| fuzzy_noise.tflite | Anomaly detection | ~88KB | ~93% |
| ble_anomaly.tflite | BLE violations | ~110KB | ~85% |
| subghz_class.tflite | Sub-GHz protocol | ~145KB | ~82% |

Download via Flipper: Models → Download from Zoo
Train custom: python3 software/pc_tools/train_model.py

Sync flow: Flipper → Models → Sync Zoo Index
→ C5 fetches zoo_index.json via Wi-Fi → saves to /sd/ml/zoo_index.json
→ User picks model → C5 downloads + SHA256 verifies → saves to /sd/ml/models/
→ RP2350B sends MODEL_LOAD to S3 → S3 loads into TFLite arena

Federated roadmap: Phase 1=centralised HuggingFace zoo. Phase 2=on-device incremental (fuzzy model). Phase 3=privacy-preserving gradient aggregation.
