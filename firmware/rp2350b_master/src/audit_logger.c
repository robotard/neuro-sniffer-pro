/**
 * audit_logger.c — Neuro-Sniffer Pro v0.3.3.0
 * RP2350B FreeRTOS task — append-only, cryptographically chained audit log.
 *
 * Every TX event, every safety block, every SEC-002 proximity action gets
 * written here. The chain property is what makes this useful as evidence
 * in a regulated security engagement rather than just a diagnostic log:
 * tampering with any single entry invalidates every entry that follows it.
 *
 * Chain mechanics:
 *   - Each entry carries a 4-byte nonce from the RP2350B hardware TRNG
 *     (get_rand_32()). Unpredictable, not seeded from anything an
 *     attacker could observe or replay.
 *   - Each entry carries prev_hash: the hardware-accelerated SHA-256 of
 *     the previous entry's raw JSON text. Modify entry N and every hash
 *     from N+1 onward stops matching — the break is immediately visible
 *     to anyone reading the log back, without needing a separate
 *     verification tool.
 *
 * See docs/decisions/ADR-009 for the full rationale, and the JSON schema
 * this produces is documented in sd_card_schema/SD_SCHEMA.md.
 *
 * File location on SD: /sd/audit/audit.log
 * Format: one JSON object per line, newline-terminated, append-only.
 */

#include "pico/stdlib.h"
#include "pico/rand.h"          // get_rand_32() / get_rand_64() — hardware TRNG
#include "hardware/sha256.h"    // RP2350B hardware SHA-256 accelerator
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ff.h"                 // FatFs — SD card filesystem
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define AUDIT_LOG_PATH      "/sd/audit/audit.log"
#define AUDIT_QUEUE_LENGTH  32          // buffer up to 32 pending log events
#define AUDIT_ENTRY_MAX_LEN 512         // matches UART command max — plenty of headroom
#define SHA256_HEX_LEN      64          // 32 bytes -> 64 hex chars
#define GENESIS_HASH        "0000000000000000000000000000000000000000000000000000000000000000"

// ────────────────────────────────────────────────────────────────────────
// The queue every other task pushes safety/security events onto. This is
// the definition — task_safety_guard.c and task_proximity_guard.c both
// declare this `extern` and push to it directly. Nothing reads it except
// this file.
// ────────────────────────────────────────────────────────────────────────
QueueHandle_t g_audit_queue = NULL;

// Must match the struct definition in task_safety_guard.c exactly —
// this is the wire format between tasks. If you change one, change both,
// or better, move this into a shared header (docs/INTERNAL_API_CONTRACT.md
// item — see API-001, this duplication is exactly the kind of thing that
// contract is meant to eliminate).
typedef struct {
    uint32_t event_type;
    uint32_t tx_power_mw;
    char     cmd_name[32];
} AuditEvent_t;

// GPS state, written by task_gps_parser.c, read here for the audit "gps"
// field. Declared extern — task_gps_parser.c owns the actual struct.
typedef struct {
    double   lat;
    double   lon;
    bool     fix;
} GpsState_t;
extern volatile GpsState_t g_gps_state;

// TEST_MODE — set at build time by FLASH_TEST.sh / FLASH_PRODUCTION.sh.
// Every audit entry is tagged with this so a reviewer can immediately
// tell alpha-bench entries apart from production-unit entries.
#ifndef TEST_MODE
#define TEST_MODE 0
#endif

// Running chain state — the hash of whatever we last wrote. Loaded from
// the last line of the existing log at boot (see audit_logger_init below),
// or set to the genesis hash if this is a brand new SD card.
static char g_prev_hash_hex[SHA256_HEX_LEN + 1] = GENESIS_HASH;
static FIL  g_log_file;
static bool g_log_open = false;

/**
 * sha256_hex — hash a null-terminated string with the RP2350B hardware
 * SHA-256 accelerator and write the result as a lowercase hex string.
 *
 * This is the whole reason we insisted on RP2350B over RP2040: on the
 * old chip this same operation ran in software and had measurable timing
 * variance depending on input length — a real (if narrow) side channel
 * for a function whose entire job is proving nothing has been tampered
 * with. The hardware block removes that variance and is roughly 40x
 * faster besides, which matters when we're hashing on every single
 * audit entry rather than just at OTA time.
 */
static void sha256_hex(const char *input, size_t len, char *out_hex) {
    sha256_state_t state;
    uint8_t digest[32];

    sha256_start(&state, SHA256_BIG_ENDIAN);
    sha256_write(&state, (const uint8_t *)input, len);
    sha256_finish(&state, digest);

    static const char hexchars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out_hex[i * 2]     = hexchars[(digest[i] >> 4) & 0xF];
        out_hex[i * 2 + 1] = hexchars[digest[i] & 0xF];
    }
    out_hex[64] = '\0';
}

/**
 * audit_logger_init — mounts the SD card, opens the log for append, and
 * — critically — reads the last line of any existing log to recover the
 * chain state. Without this step, every reboot would start a fresh chain
 * disconnected from everything before it, which defeats the point.
 *
 * Called once from app_main() before the FreeRTOS scheduler starts.
 */
bool audit_logger_init(void) {
    g_audit_queue = xQueueCreate(AUDIT_QUEUE_LENGTH, sizeof(AuditEvent_t));
    if (g_audit_queue == NULL) {
        return false; // out of heap — should never happen at this queue size
    }

    // Open for append. FA_OPEN_APPEND creates the file if it doesn't
    // exist yet (first boot on a fresh SD card).
    FRESULT fr = f_open(&g_log_file, AUDIT_LOG_PATH, FA_WRITE | FA_OPEN_APPEND);
    if (fr != FR_OK) {
        return false;
    }
    g_log_open = true;

    // Recover chain state: if the file already has content, the last
    // line's own hash becomes our starting prev_hash. If the file is
    // empty (brand new card), we stay on GENESIS_HASH and that's fine —
    // it's the documented, expected start-of-chain marker.
    //
    // We don't parse JSON here to pull out a stored hash field; instead
    // we re-derive it the same way every other entry does: hash the raw
    // line text of whatever the last entry actually was. This means the
    // chain is self-verifying from nothing but the raw file — no
    // external index or checkpoint file to also keep secure.
    FIL read_file;
    if (f_open(&read_file, AUDIT_LOG_PATH, FA_READ) == FR_OK) {
        char line[AUDIT_ENTRY_MAX_LEN];
        char last_line[AUDIT_ENTRY_MAX_LEN] = {0};
        while (f_gets(line, sizeof(line), &read_file)) {
            strncpy(last_line, line, sizeof(last_line) - 1);
        }
        f_close(&read_file);

        if (last_line[0] != '\0') {
            // Strip trailing newline before hashing, so re-derivation
            // matches exactly what was hashed when the line was written.
            size_t l = strlen(last_line);
            while (l > 0 && (last_line[l-1] == '\n' || last_line[l-1] == '\r')) {
                last_line[--l] = '\0';
            }
            sha256_hex(last_line, l, g_prev_hash_hex);
        }
    }

    return true;
}

/**
 * audit_log_append — build one JSON entry, hash-chain it to the previous
 * entry, write it, and update the running chain state. This is the only
 * function in the firmware that writes to the audit log — every other
 * task talks to us via g_audit_queue, never by touching the file directly.
 *
 * @param cmd          short command/event name (e.g. "WIFI_DEAUTH", "BLOCK_KILL_SWITCH")
 * @param params_json  a pre-formatted JSON object string for the "params" field,
 *                     or "{}" if there are none
 * @param tx_power_mw  actual TX power used, 0 for non-TX events
 */
static void audit_log_append(const char *cmd, const char *params_json, uint32_t tx_power_mw) {
    if (!g_log_open) return;

    // Hardware TRNG nonce — 4 bytes, formatted as 8 hex chars. This is
    // what makes each entry's identity unpredictable even if two entries
    // happen to have identical cmd/params/timestamp (e.g. a burst of
    // identical blocked commands in the same second) — the nonce still
    // differs, so the resulting hash still differs, so the chain still
    // has full entropy at every link.
    uint32_t nonce = get_rand_32();

    // Timestamp — placeholder ISO8601 format string. Real implementation
    // pulls wall-clock time from the RTC once task_gps_parser.c has a fix
    // (GPS gives us real UTC time); until first fix, this falls back to
    // an uptime-since-boot marker so entries are still chain-valid even
    // before GPS locks. That fallback behaviour is intentional: we'd
    // rather log with a rough timestamp than not log at all.
    char ts[32];
    uint32_t uptime_s = xTaskGetTickCount() / configTICK_RATE_HZ;
    snprintf(ts, sizeof(ts), "UPTIME+%lus", (unsigned long)uptime_s);

    // GPS block — "fix":false with lat/lon omitted-as-zero is the honest
    // representation when we don't have a lock. Never fabricate a
    // position; an audit log with a fake GPS fix is worse than one that
    // honestly says it doesn't know where it was.
    char gps_json[96];
    if (g_gps_state.fix) {
        snprintf(gps_json, sizeof(gps_json),
            "{\"lat\":%.6f,\"lon\":%.6f,\"fix\":true}",
            g_gps_state.lat, g_gps_state.lon);
    } else {
        snprintf(gps_json, sizeof(gps_json), "{\"lat\":0,\"lon\":0,\"fix\":false}");
    }

    // Assemble the full entry. Note prev_hash is whatever chain state we
    // are holding *before* this write — that's what makes it a chain
    // rather than just a list of independently-hashed entries.
    char entry[AUDIT_ENTRY_MAX_LEN];
    int len = snprintf(entry, sizeof(entry),
        "{\"ts\":\"%s\",\"gps\":%s,\"cmd\":\"%s\",\"params\":%s,"
        "\"test_mode\":%s,\"tx_mw\":%lu,\"nonce\":\"%08x\",\"prev_hash\":\"%s\"}",
        ts, gps_json, cmd, params_json,
        TEST_MODE ? "true" : "false",
        (unsigned long)tx_power_mw,
        nonce,
        g_prev_hash_hex);

    if (len <= 0 || (size_t)len >= sizeof(entry)) {
        return; // entry too long — shouldn't happen with our fixed field set,
                // but we don't want a truncated/malformed line breaking the chain
    }

    // Write + newline, then flush immediately. We accept the extra flash
    // wear cost of an immediate flush per entry because a log entry that
    // exists only in a buffer and is lost on power-cut is worse than
    // slightly worse SD card longevity — this is a security log, not a
    // performance-sensitive data stream.
    UINT written;
    f_write(&g_log_file, entry, (UINT)len, &written);
    f_write(&g_log_file, "\n", 1, &written);
    f_sync(&g_log_file);

    // Chain forward: this entry's hash becomes next entry's prev_hash.
    sha256_hex(entry, (size_t)len, g_prev_hash_hex);
}

/**
 * audit_translate_event_type — turn the numeric event_type from
 * task_safety_guard.c's AuditEvent_t into a human-readable command name
 * for the log, when the sender didn't already provide one in cmd_name.
 */
static const char *audit_translate_event_type(uint32_t event_type) {
    switch (event_type) {
        case 0xA110: return "TX_ALLOWED";
        case 1:      return "BLOCKED_KILL_SWITCH";
        case 2:      return "BLOCKED_FREQ_OUT_OF_RANGE";
        case 3:      return "BLOCKED_ES_OVERLAP";
        case 4:      return "BLOCKED_POWER_CAP";
        default:     return "UNKNOWN_EVENT";
    }
}

/**
 * task_audit_logger — pulls events off g_audit_queue as they arrive from
 * task_safety_guard.c, task_proximity_guard.c, and eventually the UART
 * bridge for user-issued commands, and writes each one to the chain.
 *
 * Priority is deliberately lower than task_safety_guard.c — the safety
 * decision itself must never be delayed waiting for a log write, so the
 * queue exists precisely to decouple "decide" from "record". A backlog
 * of a few unwritten log entries during a burst of activity is fine;
 * a delayed kill-switch response would not be.
 */
void task_audit_logger(void *pvParameters) {
    (void)pvParameters;
    AuditEvent_t ev;

    for (;;) {
        if (xQueueReceive(g_audit_queue, &ev, portMAX_DELAY) == pdTRUE) {
            // If the sender already gave us a specific cmd_name (most
            // callers do), prefer it over the generic event_type
            // translation — it's more useful in the log.
            const char *cmd = (ev.cmd_name[0] != '\0')
                ? ev.cmd_name
                : audit_translate_event_type(ev.event_type);

            audit_log_append(cmd, "{}", ev.tx_power_mw);
        }
    }
}

/**
 * audit_log_direct — for callers outside the AuditEvent_t queue path that
 * need to log something with actual structured params (e.g. a Wi-Fi
 * deauth with target/channel fields) rather than the bare "{}" the queue
 * path uses. Intended to be called from task_uart_bridge.c once the
 * external API contract is formalised (API-002) — for now the queue path
 * above covers all safety-critical events and this is the extension
 * point for richer application-level logging.
 */
void audit_log_direct(const char *cmd, const char *params_json, uint32_t tx_power_mw) {
    audit_log_append(cmd, params_json, tx_power_mw);
}
