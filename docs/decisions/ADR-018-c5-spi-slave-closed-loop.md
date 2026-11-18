# ADR-018 — ESP32-C5 SPI Slave + Closed-Loop Reply Path

- Status: Accepted
- Date: 2026-11-18
- Domains: FW, SEC
- Tags: [spi, c5, s3, ghost-tracker, protocol]

## Context

While grounding `docs/EXTERNAL_API_SPEC.md` in the real firmware (rather
than documenting an aspirational protocol), a gap bigger than "no
response format" turned up: **neither ESP32-C5 nor ESP32-S3's
`app_main()` ever initialised an SPI slave peripheral at all.**
`firmware/rp2350b_master/src/main.cpp`'s `route_cmd()` was calling
`spi_send()` — a write-only, fire-and-forget blocking SPI write — into a
bus that nothing on either co-processor was listening to as a slave.

Separately, `firmware/esp32_c5_rf/src/ghost_tracker.c` was already doing
real work: its `analysis_task()` computes motion/breathing/people-count
from CSI data and formats a `GHOST:{...}` JSON line every 500ms,
pushing it into an internal ring buffer (`spi_buf`) — but nothing ever
drained that ring buffer. The reply existed and was being thrown away.

So the real state, pre-this-ADR, was: a command sender with nothing
listening, and a reply generator with nothing reading. Two halves of a
pipeline that had never been connected.

## Decision

**ESP32-C5**: added a dedicated `spi_slave_task()` in
`firmware/esp32_c5_rf/src/main.c` using ESP-IDF's `driver/spi_slave.h`,
on the pins this file's own header comment already specified (GPIO4=CS,
GPIO5=CLK, GPIO6=MOSI, GPIO7=MISO — C5's own GPIO numbering, distinct
from RP2350B's). Each transaction is a fixed `SPI_XFER_LEN` (256) bytes,
full-duplex: the task pulls one pending line out of `ghost_tracker.c`'s
ring buffer via a new accessor, `ghost_tracker_get_reply()`, and sends
it as the transaction's TX payload — or a generic `ACK:IDLE\n` if
nothing's queued, so the master's read is always a well-formed line.
Whatever RP2350B sent as the command is logged (`ESP_LOGI`), not
dispatched — command execution isn't wired up yet (see Consequences).

**RP2350B**: `firmware/rp2350b_master/src/main.cpp`'s `route_cmd()` now
uses a new `spi_transfer_c5()` (full-duplex `spi_write_read_blocking`,
same fixed 256-byte length) for the C5-bound command classes
(`WIFI_*`, `BLE_*`, `ZIGBEE_*`, `OTA_*`, `GHOST_*`, `WARDRIVE*`), and
forwards whatever non-idle reply comes back straight to the Flipper
over UART0. The old write-only `spi_send()` is kept for the S3-bound
branch, unchanged — see below.

**Fixed-length over variable-length**: chosen because the only four
wires available (CS/CLK/MOSI/MISO) give no way to negotiate a variable
transaction length without an extra "ready"/IRQ line, which isn't in
the current pin budget. A fixed 256-byte transaction, zero-padded, is
simple and matches the Flipper-facing protocol's own 254-byte line
limit (`docs/EXTERNAL_API_SPEC.md` §1).

## Alternatives Considered

- **Half-duplex, separate write-then-read transactions**: rejected —
  ESP-IDF's `spi_slave` driver answers whatever's in its TX buffer on
  every transaction regardless; a two-phase protocol would need the
  slave to somehow know "this transaction is a read, not a write,"
  which needs either an IRQ line or a command byte scheme — more
  complexity than the alpha-stage value justifies right now.
- **Wire up S3 the same way**: rejected for this change — see
  Consequences. No GPIO pin assignment for an S3-side SPI slave exists
  anywhere in `hardware/*.md` or `docs/*.md`. Inventing pin numbers to
  make the symmetric fix would be fabricating a hardware fact this
  project has been actively correcting itself for doing elsewhere
  (see ADR-015's TrustZone-AES correction). Flagging instead.
- **Dispatch commands into real Wi-Fi actions on C5 now**: rejected for
  scope — this ADR closes the *transport* gap (nothing was listening,
  nothing was being read). Turning `DEAUTH`, `EVIL_TWIN`, etc. into
  real transmissions is separate, larger work that needs its own
  review given everything in `docs/SAFETY_COMPLIANCE.md`.

## Consequences

- **Ghost Tracker status now flows end-to-end**: Flipper → RP2350B →
  C5 → (existing Ghost Tracker data) → C5 → RP2350B → Flipper is a real,
  working loop for the first time. This is the single thing that
  actually works beyond the RP2350B-local `SAFETY_STATUS` reply.
- **Everything else forwarded to C5 is still inert**: `WIFI_SCAN_2G`,
  `DEAUTH`, `BLE_FLOOD`, etc. are received and logged on C5 but trigger
  no real action — Ghost Tracker runs unconditionally regardless of
  what's sent. Stated plainly in `docs/EXTERNAL_API_SPEC.md`, not
  glossed over.
- **S3 remains completely unreached.** `firmware/esp32_s3_ai/src/main.cpp`
  still has no SPI slave of any kind — `SUBGHZ_*`, `NFC_*`, `AI_*`,
  `MODEL_*` commands are still written into a void exactly as before
  this ADR. This needs a hardware-notes decision (an actual GPIO pin
  assignment on the S3 side) before the same fix can be mirrored there
  — flagged as a Trello card requiring that input, not silently
  deferred.
- The two chips' `SPI_XFER_LEN` constants (RP2350B's and C5's) are
  duplicated, not shared via a header, matching the existing stopgap
  pattern already called out in `main.cpp`'s top-of-file TODO about
  needing a proper shared header (`include/ctrl_role.h` per ADR-017).
  Anyone changing one must change the other.

## References
- `firmware/esp32_c5_rf/src/main.c` (`spi_slave_task`)
- `firmware/esp32_c5_rf/src/ghost_tracker.c` (`ghost_tracker_get_reply`)
- `firmware/rp2350b_master/src/main.cpp` (`spi_transfer_c5`, `route_cmd`)
- `docs/EXTERNAL_API_SPEC.md`
- `docs/decisions/ADR-017-hal-and-plane-split.md`
