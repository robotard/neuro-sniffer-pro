# Build Script Reconciliation — v0.3.3.0
# Audit of 8 uploaded BUILD_REPO*.sh files (4 unique after dedup)

## Chip references — all 4 are pre-RP2350B, superseded

| File (unique) | Lines | C6 refs | RP2040 refs | RP2350 refs |
|---|---|---|---|---|
| BUILD_REPO.sh | 1137 | 0 | 25 | 0 |
| BUILD_REPO_1-1.sh (×4 names) | 2965 | 116 | 94 | 0 |
| BUILD_REPO_MAC.sh | 1291 | 11 | 40 | 0 |
| BUILD_REPO_1-3.sh (=LINUX) | 1425 | 13 | 36 | 0 |

None are safe to run as-is. `BUILD_REPO_1-1.sh` even has a literal
`firmware/esp32_c6_rf/` directory — directly contradicts ADR-001.

## Critical finding — the ES/emergency logic contradiction

`BUILD_REPO_1-1.sh` line 887, inside its RP2040 safety task:

```c
safety.tx_allowed = safety.kill_switch_engaged && !safety.emergency_active;
```

This is a **hard, unconditional block** — TX is refused the instant
`emergency_active` is true, no override path, no professional judgement
call. This is the exact wrong model that was corrected: ES bands must
be *notify*, with override available to a configured professional, not a
silent hard stop.

**Current `task_safety_guard.c` is already correct** — its
`BLOCK_ES_OVERLAP` path is designed to be paired with a UART-bridge
confirmation dialog (default STOP, but overridable), not baked in as an
unconditional AND. No code change needed; confirms the current file, not
a regression to fix.

## Genuinely new content found — not yet in current tree

Old scripts reference these; current tree doesn't have them:

- `docs/COST_ANALYSIS.md`
- `docs/DIY_BUILD_GUIDE.md`
- `docs/MANUFACTURING_GUIDE.md`
- `docs/SAFETY_COMPLIANCE.md`
- `docs/SETUP_GUIDE.md`
- `software/pc_tools/train_model.py`

Also found: a "6-digit PIN + physical kill switch cycle" override pattern
for restricted operations — different mechanism from our current Ducky
override (header flag + audit log). Worth comparing against SEC-002
unlock (BLE proximity + PIN). Flagged, not yet resolved.

## Verdict

Old scripts are historical reference only. Current architecture (C5/S3/
RP2350B, notify-not-block ES, SEC-002) supersedes them entirely.

---
*v0.3.3.0 | reconciliation pass*