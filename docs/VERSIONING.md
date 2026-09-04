# VERSIONING.md — Neuro-Sniffer Pro v0.3.3.0
# Multi-domain versioning taxonomy

## WHY MULTI-DOMAIN

A single version number flattens decisions across six domains with
very different iteration speeds, rollback costs, and risk profiles.

## DOMAINS

| Domain | Covers | Rollback Cost |
|--------|--------|---------------|
| HW | PCB, BOM, schematic, enclosure | HIGH — board respin |
| FW | RP2350B, C5, S3 firmware | LOW — OTA/UF2 |
| SW | Flipper Lua, PC tools, build scripts | TRIVIAL — git revert |
| ML | TFLite models, training, zoo | LOW — OTA rollback |
| SEC | Kill switch, whitelist, audit, signed boot | CRITICAL — never regress |
| DOC | Architecture, ADRs, test plans | NONE |

## FORMAT

Domain: DOMAIN-MAJOR.MINOR.PATCH
Master tag: NSP-0.3.3.0 = { HW, FW, SW, ML, SEC, DOC }

## CURRENT — NSP-0.3.3.0

| Domain | Version | Last change |
|--------|---------|-------------|
| HW | 0.1.0 | Three-board modular system finalised |
| FW | 0.3.3 | RP2350B upgrade — SHA-256, TRNG, TrustZone |
| SW | 0.2.0 | Unified BUILD_REPO.sh, FLASH_TEST/PRODUCTION |
| ML | 0.1.0 | Six model architecture, no real-hardware training yet |
| SEC | 0.3.0 | RP2350B signed boot roadmap, TRNG audit nonce chain |
| DOC | 0.3.3 | ADR system adopted, versioning taxonomy established |

## PRE-RELEASE CONVENTION

0.x.x.x — pre-alpha (no physical board)
0.x.x.1 — first board boots (HW increments to 0.1.1)
1.0.0.0 — public open source release

## MILESTONE MAP

NSP-0.3.3.0  RP2350B upgrade + ADR system  ← HERE
NSP-0.3.3.1  First board boots              ← NEXT
NSP-0.4.0.0  Post-alpha corrections
NSP-0.5.0.0  fuzzy_noise engine complete
NSP-0.6.0.0  All models trained on real hardware
NSP-0.9.0.0  Pre-release hardening, signed boot enabled
NSP-1.0.0.0  Public open source release