# Neuro-Sniffer Pro v0.3.3.0 — Podcast Transcript
# Episode 1: "Why Everything Else Is a Toy"

```typescript
interface Episode {
  runtime: "~15 min";
  voice:   "Synthetic TTS (ffmpeg/flite, slt voice)";
  file:    "media/neuro_sniffer_pro_podcast.mp3";
}
```

---

## [00:00] Intro

Welcome to the Neuro-Sniffer Pro podcast. The AI-driven RF research platform
built for Flipper Zero — built for professionals hardening mission-critical
wireless devices before deployment.

---

## [00:20] Competition

```typescript
const market: Product[] = [
  { name: "Flipper Wi-Fi module", price: 29,  fiveGhz: false, ai: false, subGhz: false },
  { name: "Marauder ESP32",       price: 50,  fiveGhz: false, ai: false, subGhz: false },
  { name: "HackRF One",          price: 340, fiveGhz: false, ai: false, subGhz: true  },
  { name: "Neuro-Sniffer Pro",   price: 89,  fiveGhz: true,  ai: true,  subGhz: true  },
];
```

---

## [01:30] Chip Architecture

**C5 not C6.** C6 = single-band 2.4GHz, 160MHz. C5 = dual-band 2.4+5GHz, 240MHz, #1 CSI in ESP32 family. ESP-IDF v5.5 mandatory.

**S3** = TFLite AI, SX1262 Sub-GHz 150-960MHz, PN532 NFC, 8MB PSRAM, emergency kill GPIO.

**RP2350B** = every command routes through it, every safety check runs here at 50ms. Hardware SHA-256, TRNG, TrustZone.

---

## [02:45] Value Add #1 — Dual-Band Wi-Fi 6

No other Flipper module sees 5GHz. Most enterprise WPA3 deployments are 5GHz primary.

---

## [03:30] Value Add #2 — Ghost Tracker

```typescript
const ghostTracker: PassiveSensor = {
  transmission: false,
  subcarriers:  52,
  sampleRate:   "20Hz",
  capabilities: {
    motionDetection:  "~95% at 5m through one wall",
    breathingRate:    "Goertzel filter 0.15-0.60Hz (9-36 BPM), ~90% accuracy",
    personID:         "~99.82% (transformer model, static subjects)",
    crowdCount:       "~79%, ±1 person up to 10 occupants",
    rfFingerprinting: "CFO + clock skew + spectral flatness — survives MAC randomisation",
    triangulation:    "2+ units + UMAP channel charting → 2D map, ~0.5-1.5m NLOS",
  },
};
```

---

## [06:00] Value Adds #3–10

**#3 AI On-Device** — 6 TFLite Micro models, <200KB each, local inference, no cloud.

**#4 Sub-GHz 150-960MHz** — SX1262 capture/replay/sweep.

**#5 NFC/RFID** — PN532: Mifare/NTAG/DESFire read, write, clone.

**#6 GPS Wardrive** — NEO-M9N GNSS, every TX gets a GPS stamp.

**#7 OTA + Auto-Rollback** — Dual-bank, 60s watchdog, no programmer after initial flash.

**#8 Hardware Safety** — Physical kill switch, emergency shield (notify not block), frequency whitelist, geo-fence, append-only audit log.

**#9 Neuro-Ducky** — RP2350B USB-HID BadUSB, whitelisted categories.

**#10 Open Source** — MIT firmware, CC-BY-NC-SA models.

---

## [12:00] Closing

*Neuro-Sniffer Pro. See the invisible. Harden the critical. Stay legal.*

---

## [12:15] Legal Disclaimer

Passive RF monitoring is zero-TX and lawful in most jurisdictions. Active transmission is lawful only on systems you own or have explicit written authorisation to test. Restricted frequencies are illegal to transmit on under UK/US/EU law. Safety systems are engineering safeguards, not legal authorisation. Authors accept zero liability. Stay safe. Stay legal. Hack responsibly.