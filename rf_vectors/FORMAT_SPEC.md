# Neuro RF Vector Format Specification v1.0
# File extension: .nrfv
# Encoding: UTF-8 JSON
# github.com/robotard/neuro-sniffer-pro

---

## SCHEMA

```json
{
  "nrfv_version": "1.0",
  "name": "Human-readable signal name",
  "description": "What this signal is and what device produces it",
  "author": "Contributor name or handle",
  "licence": "CC-BY-NC-SA",
  "created": "2025-01-15T00:00:00Z",
  "band": "subghz | wifi | ble | zigbee | lf | hf",
  "frequency_hz": 433920000,
  "modulation": "OOK | FSK | LoRa | BLE_ADV | 802.15.4 | ISO14443A | EM4100",
  "protocol": "Optional: protocol name if known",
  "purpose": "IDENTIFY",
  "device_class": "medical | industrial | consumer | building | rescue | unknown",
  "signatures": [
    {
      "type": "preamble_pattern | sync_word | frame_structure | timing",
      "value": "0xAA 0x55 ...",
      "notes": "Human-readable description of this signature element"
    }
  ],
  "caveats": "Any jurisdictional, safety, or accuracy notes"
}
```

---

## PURPOSE FIELD — MANDATORY

The `purpose` field MUST be `"IDENTIFY"`.

This field signals that the vector teaches the tool to recognise a signal,
not to attack or exploit it. Vectors with any other purpose will be rejected
from community contributions without review.

The distinction: "this packet structure identifies a Philips medical telemetry
device" is identification. "This byte sequence unlocks the device" is
exploitation. Only the former belongs in this library.

---

## DEVICE CLASS FIELD

Used to filter vectors by deployment context:
- `medical` — medical devices (WMTS, MedRadio, Bluetooth medical profiles)
- `industrial` — ICS, SCADA, building automation (Modbus RF, DALI, KNX)
- `consumer` — general consumer electronics
- `building` — access control, HVAC, lighting (Wiegand, KNX, Zigbee HA)
- `rescue` — search and rescue, emergency (non-ES, civilian SAR equipment)
- `unknown` — unidentified device type

---

## COMMUNITY CONTRIBUTION PROCESS

1. Fork github.com/robotard/neuro-sniffer-pro
2. Add your .nrfv file to rf_vectors/community/
3. Open a pull request with:
   - Device tested against (model/manufacturer if known)
   - Test environment (your own device, written permission obtained)
   - Accuracy notes (how reliably does this signature match?)
4. Maintainer review: purpose field check, accuracy claim review, safety check
5. Merge to rf_vectors/community/ under CC-BY-NC-SA

**PRs containing exploitation content will be closed without review.**
No rolling code attack sequences. No protocol crack tools. No brute-force
credential lists. This is a diagnostic library, not an arsenal.

---

## EXAMPLE — VALID NRFV FILE

```json
{
  "nrfv_version": "1.0",
  "name": "Generic 433MHz OOK keyfob",
  "description": "Common fixed-code OOK keyfob as used in building access and consumer remote controls. PT2262/PT2272 encoder family.",
  "author": "robsta",
  "licence": "CC-BY-NC-SA",
  "created": "2025-01-15T00:00:00Z",
  "band": "subghz",
  "frequency_hz": 433920000,
  "modulation": "OOK",
  "protocol": "PT2262",
  "purpose": "IDENTIFY",
  "device_class": "building",
  "signatures": [
    {
      "type": "preamble_pattern",
      "value": "0x00 0x00 0x00 0x00",
      "notes": "4-byte zero preamble before PT2262 sync pulse"
    },
    {
      "type": "timing",
      "value": "sync_high=4000us sync_low=124us bit_1=1000us bit_0=330us",
      "notes": "PT2262 standard timing at 433.92MHz"
    }
  ],
  "caveats": "Fixed-code only. Rolling code variants will not match this signature. Frequency varies ±75kHz by region and device."
}
```

---

*v0.3.3.0 | CC-BY-NC-SA | github.com/robotard/neuro-sniffer-pro*
