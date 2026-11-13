# BENCH_TEST_ALPHA.md — Neuro-Sniffer Pro v3.2.0
# Advanced Bench Test Procedures — INTERNAL USE ONLY
# FOR QUALIFIED PERSONNEL IN RF-SHIELDED ENVIRONMENTS ONLY
# github.com/robotard/neuro-sniffer-pro

---

## ⚠ MANDATORY PREREQUISITES — READ BEFORE PROCEEDING

This document covers testing of capabilities that carry meaningful risk of:
- RF interference with third-party devices outside the test environment
- Unlawful transmission if performed outside a shielded enclosure
- Damage to test equipment if power budgets are exceeded
- Legal liability if performed without written authorisation

**Every test in this document requires ALL of the following:**

```
[ ] RF-shielded enclosure (Faraday cage) confirmed closed and sealed
[ ] RF leakage test passed (<-60dBm outside cage at test frequencies)
[ ] Written authorisation on file for all target devices
[ ] Audit log running and GPS lock confirmed
[ ] Kill switch within arm's reach and confirmed functional
[ ] A second person present (buddy system — mandatory for high-power TX tests)
[ ] TEST_MODE=true confirmed in Flipper status bar [TEST]
[ ] TX power: 10mW cap confirmed (production bench: up to 100mW with explicit override)
[ ] No emergency service equipment within 10m of test environment
```

If ANY box above cannot be checked: DO NOT PROCEED.

---

## ALPHA BENCH ENVIRONMENT SPECIFICATION

### Minimum Acceptable Environment
- **Faraday cage**: minimum 40dB attenuation at 2.4GHz, 5GHz, 433MHz, 868MHz
- **Internal space**: minimum 60×40×40cm (board + target device + measurement equipment)
- **Access ports**: SMA bulkhead feedthroughs for external antenna/measurement connections
- **Monitoring**: Spectrum analyser or SDR outside cage to confirm containment
- **Ventilation**: Passive only (no gaps exceeding 1/10 wavelength at highest test frequency)

### Recommended Test Equipment
- Spectrum analyser (minimum span: 100MHz–6GHz)
- SDR (RTL-SDR or HackRF) as secondary monitor outside cage
- Oscilloscope for power rail measurements
- Current probe (RP2350B I2C BQ25895 monitoring can substitute for most power measurements)
- RF attenuator set (6dB, 10dB, 20dB) for high-power path validation

### Target Device Requirements
- Own device or device under test with written engagement permission
- Target device must be self-contained — no external network connections during test
- If target is medical device or safety-critical hardware: patient simulation mode active,
  no connection to any real patient monitoring infrastructure

---

## SECTION A — fuzzy_noise ENGINE STRESS TESTS

The fuzzy_noise engine has two distinct functions:
1. **Anomaly detection** — learns baseline RF profile, flags unknown deviations
2. **Controlled noise generation** — shaped adversarial RF stress for resilience testing

Both require shielded environment. Noise generation requires written permission for target.

### A.1 Anomaly Detection Baseline Training
```
Environment: Shielded cage. Target device active. No other RF sources inside cage.
Duration: 5 minutes minimum for stable baseline.
```

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Start baseline capture | "FUZZY_BASELINE: TRAINING" in Flipper status | | |
| Baseline converges | "FUZZY_BASELINE: STABLE" after 5 min | | |
| Baseline saved to SD | /sd/models/baseline_DEVICENAME.fbl written | | |
| TEST_MODE: anomaly scores | Raw variance scores per subcarrier visible | | |
| Known protocol annotated | Target device protocol identified by rf_classifier.tflite | | |

### A.2 Anomaly Detection — Known Injection
```
Environment: Shielded cage. Baseline trained (A.1 complete).
Inject: Known RF anomaly (replay capture from SD at 10mW inside cage).
```

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Inject known anomalous signal | fuzzy_noise score rises above threshold (0.7) | | |
| Alert generated | "ANOMALY DETECTED: score 0.84" on Flipper | | |
| Alert logged | Entry in /sd/audit/audit.log with signal details | | |
| False positive rate | 10 min clean baseline with no injections: zero alerts | | |
| Score decay | Alert score decays back to baseline within 30s of injection stop | | |

### A.3 Anomaly Detection — Unknown Signal (Novel Vector)
```
Environment: Shielded cage. Baseline trained (A.1 complete).
Inject: Signal type NOT present during baseline training.
This simulates an attacker using an unknown protocol against the target.
```

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Novel signal injected | fuzzy_noise detects without prior signature match | | |
| Flagged as UNKNOWN_VECTOR | Alert type: UNKNOWN rather than known signature | | |
| Novel signal captured | Raw IQ saved to /sd/captures/unknown_YYYYMMDD.iq | | |
| subghz_class.tflite runs | Best-guess protocol classification attempted | | |
| TEST_MODE: full signal delta | Per-subcarrier deviation from baseline displayed | | |

### A.4 Controlled Noise Generation — RF Stress Test
```
Environment: Shielded cage MANDATORY. Target device under test inside cage.
Permission: Written authorisation for target device required.
TX power: 10mW (TEST). Do not exceed without documented override.
Duration: Start with 10-second bursts. Extend only if target survives.
```

> This is the core mission critical device stress test function.
> The noise generated is shaped and controlled — not a blunt jammer.
> The intent is to find the frequency and pattern at which the target
> device's comms degrade, so those weaknesses can be addressed by the manufacturer.

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Noise profile: AWGN (additive white Gaussian) | Target device shows degraded SNR, comms stressed | | |
| Noise profile: Narrowband interference | Specific subcarrier interference isolated | | |
| Noise profile: Burst interference | Periodic burst at configurable duty cycle | | |
| Noise profile: Protocol-aware fuzzing | Bit-flip injection on decoded protocol frames | | |
| Target device response logged | Target comms state (up/degraded/failed) logged per noise type | | |
| TX stop on kill switch | All noise generation halts within 50ms of kill switch | | |
| TX stop on ES detection | If ES monitor triggers during noise gen: auto-halt + alert | | |
| Power budget confirmed | RP2350B BQ25895 reading confirms TX within power cap | | |
| Heat check at 60 min | C5 and S3 temperatures within spec (thermal probe or IR gun) | | |

### A.5 Dual Sub-GHz Simultaneous TX+RX (PowerPack Only)
```
Environment: Shielded cage. Two SX1262 modules active simultaneously.
SX1262_A: RX on target frequency (capture/monitor)
SX1262_B: TX on adjacent/interfering frequency
```

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Dual SX1262 init | Both report ONLINE, separate CS confirmed | | |
| Simultaneous TX+RX | A captures while B transmits without mutual interference | | |
| SX1262 isolation confirmed | A's RSSI not swamped by B's TX | | |
| Rolling code analysis | Capture on A while B generates de Bruijn sequence on adjacent channel | | |
| Frequency separation minimum | Determine minimum safe separation for simultaneous TX+RX | | |

---

## SECTION B — POWER STRESS TESTS

### B.1 Peak Current Draw — All RF Active Simultaneously
```
Environment: Bench (not necessarily shielded — passive caps reduce TX)
Measurement: Current probe on 3.3V_MAIN rail OR BQ25895 I2C telemetry
```

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Spine passive max draw | C5 CSI + S3 inference + RP2350B active: <400mA @ 3.3V | | |
| Spine active TX peak | C5 TX + S3 active + RP2350B: <950mA @ 3.3V (Flipper GPIO limit) | | |
| Flipper GPIO brownout | Confirm Flipper does NOT brownout during our peak draw | | |
| Spine USB-C path | Our USB-C feeds load — Flipper GPIO contribution drops to near-zero | | |
| PowerPack 18W budget | Full active TX: confirmed within 18W at 9V/2A PD input | | |
| LiPo discharge curve | Discharge from 100% to 20% under load — curve logged to SD | | |
| OTG to Flipper confirmed | Flipper charges at 1A from our OTG during passive ops | | |
| Dynamic load management | RP2350B reduces charge rates during TX peak automatically | | |

### B.2 Thermal Stress (60-Minute Active TX)
```
Environment: Shielded cage. All RF active at 10mW. Ambient temp logged.
```

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| C5 temperature at 60 min | <85°C (ESP32-C5 Tj max) | | |
| S3 temperature at 60 min | <85°C | | |
| RP2350B temperature at 60 min | <85°C | | |
| BQ25895 temperature | <70°C (thermal protection triggers at 80°C) | | |
| LiPo temperature | <45°C (LiPo charge safe limit) | | |
| Case temperature (external) | <45°C (user-touch safe limit) | | |
| Throttling observed | Note any TX power reduction due to thermal throttle | | |

---

## SECTION C — HARDWARE SAFETY VALIDATION

### C.1 Kill Switch Response Time
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Kill switch during active TX | RF VCC cut within 50ms | | |
| Kill switch during Sub-GHz TX | SX1262 TX halted within 50ms | | |
| Kill switch during fuzzy_noise | All noise generation halted within 50ms | | |
| Kill switch restore | RF VCC restored, chips re-enumerate, Flipper shows "RF ONLINE" | | |
| Kill switch under power stress | Kill switch functions at peak current draw | | |

### C.2 Frequency Whitelist Enforcement
| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| TX within whitelist (433MHz) | TX proceeds normally | | |
| TX outside whitelist (attempt 100MHz) | RP2350B blocks, logs "TX_WHITELIST_BLOCK" | | |
| TX outside whitelist (attempt 2.4GHz with wrong config) | Block enforced | | |
| Whitelist bypass attempt via raw UART | RP2350B rejects malformed command, logs attempt | | |

### C.3 Emergency Service Monitor Under Active TX
```
Environment: Shielded cage. Active TX running. ES signal injected via cage bulkhead.
```

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| ES detection during passive ops | Notification only, no TX interruption | | |
| ES detection during active TX | Confirmation dialog: default STOP after 10s | | |
| User selects STOP | TX halts immediately, event logged | | |
| User selects YES (override) | TX continues, override logged with GPS + timestamp | | |
| ES monitor false positive rate | Known clean Sub-GHz traffic: zero false ES alerts | | |

---

## SECTION D — MULTI-SOURCE RF FUSION ACCURACY

### D.1 Fusion vs CSI-Only Comparison
```
Environment: Known test scenario — one person seated at 2m, one standing at 4m.
Run CSI-only Ghost Tracker, then full fusion engine. Compare results.
```

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| CSI-only person count | ≥1 person detected | | |
| Fusion person count | ≥2 persons detected (higher confidence) | | |
| Sub-GHz contribution | Removing SX1262 source reduces confidence score | | |
| BLE contribution | Phone present: BLE density increases occupancy estimate | | |
| Zigbee contribution | Zigbee device active: 802.15.4 traffic confirms presence | | |
| Fusion accuracy improvement | Document % improvement over CSI-only for test report | | |

### D.2 Low-Wi-Fi Environment (Mission Critical Simulation)
```
Simulate RF-quiet medical device environment: suppress Wi-Fi traffic.
Ghost Tracker must still function via non-CSI sources.
```

| Test | Expected | Pass | Notes |
|------|----------|------|-------|
| Wi-Fi traffic suppressed | No usable CSI frames (simulate quiet ward) | | |
| Sub-GHz still detects person | 433MHz/868MHz activity from person's devices | | |
| BLE still detects person | Phone/wearable BLE advertisements | | |
| Fusion still shows presence | Overall confidence >0.5 without CSI | | |
| Alert: CSI unavailable | Flipper shows "CSI: NO SIGNAL — using fusion only" | | |

---

## SECTION E — DOCUMENTED SHORTCOMINGS AND FRINGE CASES

### E.1 Known Limitations (Current Architecture)

**Jurisdictional flags — consult legal before testing:**
- **Sub-GHz replay**: Lawful only on own devices. Rolling code replay on third-party
  systems is potentially unlawful under Computer Misuse Act 1990 (UK) and equivalent.
  Document written permission explicitly.
- **Deauth frames**: IEEE 802.11w (PMF/MFP) renders deauth ineffective against
  protected management frame-enabled APs. Many modern enterprise APs are PMF-required.
  This is a capability gap, not a bug. Note in client reports.
- **5GHz Ghost Tracker**: IDFGH-17558 means 5GHz CSI is non-functional until patched IDF.
  All alpha testing on 2.4GHz only. 5GHz human sensing deferred.
- **GPS indoors**: NEO-M9N requires clear sky view. Indoor testing will show no GPS fix.
  Audit logs will show GPS:NO_FIX. Acceptable for alpha. Dead reckoning or manual
  coordinate entry is a v4.0 feature.
- **LF RFID coil tuning**: AS3911B LF coil requires bench tuning for optimal read range.
  Alpha units may show reduced LF range until coil inductance is finalised.
- **NFC DESFire EV3**: AS3911B supports EV3 at protocol level but our firmware currently
  only implements EV1/EV2 application enumeration. EV3 full support is v3.3 feature.

**Capability gaps vs full RF research lab:**
- No spectrum below 150MHz (SX1262 lower limit). AM/FM/LW/MW bands not covered.
- No signal generation above 960MHz outside of Wi-Fi/BLE bands (no LTE/5G uplink capability — intentional exclusion, not a gap).
- No real-time IQ streaming to PC (SD card only for alpha). USB 2.0 bandwidth on RP2350B limits this. v4.0 feature.
- No 802.11ad/ay (60GHz WiGig). Outside scope entirely.
- fuzzy_noise anomaly model accuracy degrades in very high-density RF environments (>50 simultaneous Wi-Fi APs). Noted for future model retraining.

**Fringe/jurisdictional considerations (do not implement without legal review):**
- **Geo-fence enforcement**: GPS zone-based TX blocking is technically implemented but
  the zone database does not yet exist. This feature is infrastructure-only in v3.2.
  Do not advertise as functional until zone database is populated and verified.
- **LTE uplink/downlink sensing**: Passive detection of LTE traffic patterns is technically
  possible with SX1262 but is NOT implemented and NOT planned. LTE is a licensed band
  and passive monitoring creates legal ambiguity in several jurisdictions.
- **Bluetooth Classic (BR/EDR)**: ESP32-C5 supports BLE 5 only. Bluetooth Classic
  (legacy) is NOT supported. Medical devices using SPP profiles are not accessible.
  This is a real capability gap for older medical device testing.
- **Wi-Fi 7 (802.11be)**: Not supported by C5. Wi-Fi 7 uses 6GHz band. No current
  Flipper module supports this. Note gap for future hardware revision.
- **Faraday cage certification**: Alpha testing can proceed without a certified cage
  but production use for compliance testing (medical device CE marking support)
  may require tests performed in a certified RF-shielded environment. Flag to clients.

---

## POST-TEST PROCEDURE

After every bench test session:

```
[ ] Kill switch engaged — RF VCC cut
[ ] Audit log downloaded from SD and archived
[ ] SD card captures backed up and labelled with date + test ref
[ ] Firmware version and TEST_MODE flag logged in session notes
[ ] Any anomalies, unexpected behaviour, or test failures documented
[ ] Faraday cage RF leakage re-tested (confirm containment was maintained)
[ ] Equipment powered down in order: Flipper → Spine/Sandwich → external power
```

---

*v3.2.0 | INTERNAL ALPHA ONLY — DO NOT DISTRIBUTE*
*robotard | Robustas Design & Engineering | github.com/robotard/neuro-sniffer-pro*
