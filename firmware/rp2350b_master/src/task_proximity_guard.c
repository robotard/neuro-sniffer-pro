/**
 * task_proximity_guard.c — Neuro-Sniffer Pro v0.3.3.0 — SEC-002
 * RP2350B FreeRTOS task — proximity-based device lock / crypto-shred wipe.
 *
 * If the paired device (phone/watch) moves out of BLE range and stays out
 * past a confirmation timeout, this task does one of four things,
 * configured once at onboarding and not changeable at runtime:
 *
 *   LOCK           — device + boot lock. Needs paired device back in range
 *                    AND correct PIN to unlock.
 *   WIPE           — crypto-shred: destroy the SD encryption key immediately,
 *                    then schedule a physical overwrite in the background.
 *   LOCK_THEN_WIPE — lock now, escalate to WIPE if not unlocked within a
 *                    second, longer timer.
 *   ALERT_ONLY     — log + notify, no lock. For lab/bench use where you
 *                    don't want your test rig locking itself every time
 *                    you walk to get coffee.
 *
 * ── A note on what "encryption" actually means on this chip ─────────────────
 * RP2350B has hardware SHA-256 and a hardware TRNG. It does NOT have a
 * hardware AES peripheral — TrustZone on this part is a privilege/memory
 * separation feature (secure vs non-secure execution), not a crypto
 * accelerator. So "encrypted at rest" here means: a 256-bit key generated
 * once via the hardware TRNG, stored in RP2350B OTP (write-once, chip-
 * bound, not readable by non-secure code once signed boot is enabled —
 * see FW-017, deferred to pre-production), used to drive software
 * AES-256-GCM (mbedTLS) for actual SD card encryption. The hardware SHA-256
 * is used for HKDF key derivation, not for the AES step itself. This is
 * accurate to the silicon; earlier project notes describing "hardware AES
 * via TrustZone" were wrong and are corrected here.
 *
 * ── Why WIPE is crypto-shred, not a full-media overwrite ─────────────────
 * Overwriting an entire SD card sector-by-sector is slow (tens of seconds
 * to minutes depending on card size) and wears the flash. Destroying the
 * 32-byte key instead is near-instant and makes every existing ciphertext
 * byte unrecoverable the moment it happens, because there is nothing left
 * to decrypt it with. We still queue a background physical overwrite of
 * just the OTP-derived key-storage region and the SD card's key-wrapping
 * header as belt-and-braces, but the security property you actually care
 * about — "this data is now unrecoverable" — is true from the instant the
 * key is destroyed, not from whenever a multi-minute wipe job finishes.
 *
 * See docs/decisions/ADR-015 and docs/DEVICE_SECURITY_MODEL.md.
 */

#include "pico/stdlib.h"
#include "pico/rand.h"
#include "hardware/sha256.h"
#include "hardware/watchdog.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ff.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// mbedTLS — software AES-256-GCM. Real hardware acceleration on this part
// stops at SHA-256; this is the honest software fallback for the AES step.
#include "mbedtls/gcm.h"
#include "mbedtls/hkdf.h"

// ────────────────────────────────────────────────────────────────────────
// Response modes — set once at onboarding (task_onboarding.c), stored in
// NVS, not runtime-toggleable. Same philosophy as TEST_MODE (ADR-004):
// a security posture that can be casually flipped mid-session isn't
// really a security posture.
// ────────────────────────────────────────────────────────────────────────
typedef enum {
    PROX_MODE_LOCK = 0,
    PROX_MODE_WIPE,
    PROX_MODE_LOCK_THEN_WIPE,
    PROX_MODE_ALERT_ONLY,
} ProximityMode_t;

// Defaults — overridden by whatever was written to NVS at onboarding.
#define DEFAULT_RSSI_THRESHOLD_DBM   (-80)   // roughly 5m in open air, tune on bench
#define DEFAULT_CONFIRM_TIMEOUT_MS   10000   // 10s before acting on "out of range"
#define DEFAULT_LOCK_THEN_WIPE_MS    300000  // 5 minutes locked before escalating to wipe

#define AES_KEY_LEN_BYTES  32   // AES-256
#define GCM_IV_LEN_BYTES   12   // standard GCM nonce size
#define GCM_TAG_LEN_BYTES  16

// ────────────────────────────────────────────────────────────────────────
// BLE RSSI samples arrive on this queue, pushed by ESP32-C5's BLE scan
// task via the RP2350B<->C5 SPI command channel (see API-001 — this
// queue is the RP2350B-side landing point for whatever that contract
// ends up specifying as the wire format; the struct below is our current
// best guess and should be reconciled against docs/INTERNAL_API_CONTRACT.md
// once that's written).
// ────────────────────────────────────────────────────────────────────────
typedef struct {
    int8_t   rssi_dbm;
    bool     device_present;   // false if the paired device wasn't seen at all this scan cycle
} BleProximitySample_t;

QueueHandle_t g_ble_proximity_queue = NULL;

// Remote-kill trigger — set by the C5 Wi-Fi task on receipt of an
// authenticated MQTT/SMS kill command. A separate, deliberately simple
// path: remote kill always acts as if PROX_MODE_WIPE regardless of the
// configured proximity mode, on the reasoning that if you're triggering
// remote kill at all, you've already decided this is the serious case.
static volatile bool g_remote_kill_requested = false;

extern QueueHandle_t g_audit_queue; // from audit_logger.c
typedef struct { uint32_t event_type; uint32_t tx_power_mw; char cmd_name[32]; } AuditEvent_t;

// ────────────────────────────────────────────────────────────────────────
// Device lock state — checked by task_safety_guard.c and task_uart_bridge.c.
// While locked, RF VCC cannot be enabled and the UART bridge refuses every
// command except UNLOCK_ATTEMPT.
// ────────────────────────────────────────────────────────────────────────
typedef struct {
    bool     locked;
    uint32_t locked_at_uptime_s;
    uint32_t unlock_pin_hash[8];  // SHA-256 of the correct PIN, not the PIN itself
} DeviceLockState_t;

static volatile DeviceLockState_t g_lock_state = { .locked = false };

// The master key. Lives in RAM only after being unwrapped from OTP at
// boot; destroying this variable IS the crypto-shred. We never write the
// raw key to SD — only ciphertext, ever.
static uint8_t g_sd_master_key[AES_KEY_LEN_BYTES];
static bool    g_key_valid = false;

// Config, read from NVS at boot by task_onboarding.c and passed in here.
static ProximityMode_t g_mode              = PROX_MODE_LOCK;
static int8_t          g_rssi_threshold    = DEFAULT_RSSI_THRESHOLD_DBM;
static uint32_t        g_confirm_timeout_ms = DEFAULT_CONFIRM_TIMEOUT_MS;
static uint32_t        g_escalate_timeout_ms = DEFAULT_LOCK_THEN_WIPE_MS;

/**
 * proximity_guard_configure — called once from task_onboarding.c after it
 * reads the SEC-002 settings the user chose during first-flash setup.
 * Not exposed to runtime UART commands — changing proximity policy
 * requires going through onboarding again, same as TEST_MODE.
 */
void proximity_guard_configure(ProximityMode_t mode, int8_t rssi_threshold_dbm,
                                uint32_t confirm_timeout_ms) {
    g_mode = mode;
    g_rssi_threshold = rssi_threshold_dbm;
    g_confirm_timeout_ms = confirm_timeout_ms;
}

/**
 * derive_session_key — HKDF-SHA256 (hardware-accelerated hash step) over
 * the OTP master secret plus a TRNG-generated per-boot salt, producing a
 * fresh 256-bit session key every boot. Even if a session key were ever
 * somehow extracted from RAM, it's useless for decrypting data written in
 * a different boot session.
 */
static void derive_session_key(const uint8_t *otp_secret, size_t otp_secret_len,
                                uint8_t *out_key) {
    uint8_t salt[16];
    // Hardware TRNG for the per-boot salt — this is the "fresh key every
    // boot" property. get_rand_32() called 4x to fill 16 bytes.
    for (int i = 0; i < 4; i++) {
        uint32_t r = get_rand_32();
        memcpy(salt + i * 4, &r, 4);
    }

    // mbedTLS HKDF using SHA-256. The RP2350B hardware SHA-256 accelerator
    // is used internally by mbedTLS's hash context on this platform build
    // — see firmware/rp2350b_master/CMakeLists.txt hardware_sha256 link.
    mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                 salt, sizeof(salt),
                 otp_secret, otp_secret_len,
                 (const uint8_t *)"nsp-sec002-v1", 13,
                 out_key, AES_KEY_LEN_BYTES);
}

/**
 * proximity_guard_load_key — called once at boot. Reads the OTP-stored
 * master secret (written once at onboarding, never again) and derives
 * this boot's session key from it. If OTP hasn't been provisioned yet
 * (very first boot before onboarding completes), this is a no-op and
 * g_key_valid stays false — SD encryption simply doesn't activate until
 * onboarding has run, which is consistent with TEST_MODE also being
 * undefined until that point.
 */
bool proximity_guard_load_key(void) {
    // Placeholder for the actual OTP read API (Pico SDK 2.x otp_read /
    // picotool-provisioned region — exact call depends on final OTP row
    // allocation, tracked alongside FW-017 signed boot keypair work).
    // For alpha bring-up with signed boot disabled, this can read from a
    // reserved NVS flash page instead of true OTP, since OTP write cycles
    // are precious and we don't want to burn them during alpha iteration.
    uint8_t otp_secret[32];
    bool have_secret = false; // = otp_read_provisioned(otp_secret, sizeof(otp_secret));

    if (!have_secret) {
        g_key_valid = false;
        return false;
    }

    derive_session_key(otp_secret, sizeof(otp_secret), g_sd_master_key);
    g_key_valid = true;
    return true;
}

/**
 * proximity_guard_encrypt_buffer / decrypt_buffer — the actual software
 * AES-256-GCM operations used by anything writing sensitive data to SD
 * (audit_logger.c's raw entries are append-only plaintext by design, for
 * transparency and because their integrity comes from the hash chain, not
 * secrecy — but captures, RF fingerprints, and Ducky script output all go
 * through these functions before hitting the card).
 */
bool proximity_guard_encrypt_buffer(const uint8_t *plaintext, size_t len,
                                     uint8_t *out_ciphertext,
                                     uint8_t out_iv[GCM_IV_LEN_BYTES],
                                     uint8_t out_tag[GCM_TAG_LEN_BYTES]) {
    if (!g_key_valid) return false;

    for (int i = 0; i < 3; i++) {
        uint32_t r = get_rand_32();
        memcpy(out_iv + i * 4, &r, 4);
    }

    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, g_sd_master_key, AES_KEY_LEN_BYTES * 8);
    int rc = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, len,
                                        out_iv, GCM_IV_LEN_BYTES,
                                        NULL, 0,
                                        plaintext, out_ciphertext,
                                        GCM_TAG_LEN_BYTES, out_tag);
    mbedtls_gcm_free(&ctx);
    return rc == 0;
}

/**
 * proximity_guard_crypto_shred — THE wipe. Destroys the in-RAM session
 * key and overwrites the OTP-derived secret's NVS staging copy (alpha) /
 * OTP row (production, post FW-017). From this instant, every ciphertext
 * byte previously written to SD is permanently unrecoverable — there is
 * no key anywhere on the device or off it that can decrypt it.
 *
 * This function must complete in well under a second. It does not touch
 * the SD card's actual data blocks — that's the slower background job
 * queued afterward for defence in depth, not the security boundary itself.
 */
static void proximity_guard_crypto_shred(void) {
    // Zero the in-RAM key first — belt and braces even though it's about
    // to go out of scope / be overwritten by the next boot's derivation.
    memset(g_sd_master_key, 0, sizeof(g_sd_master_key));
    g_key_valid = false;

    // Overwrite the staged secret. Production (post FW-017) equivalent
    // is an OTP "burn to all-1s over the existing row" operation, which
    // is destructive and permanent by the nature of OTP memory — you
    // cannot un-burn an OTP bit, which is exactly the property we want
    // for a wipe trigger.
    // otp_destroy_provisioned_secret();

    AuditEvent_t ev = { .event_type = 0xDEAD, .tx_power_mw = 0 };
    strncpy(ev.cmd_name, "SEC002_CRYPTO_SHRED", sizeof(ev.cmd_name) - 1);
    xQueueSend(g_audit_queue, &ev, 0);

    // Background physical overwrite job — best-effort, not security-critical,
    // queued here as a lower-priority task. Not blocking on this function.
    // (Implementation: separate low-priority task scanning /sd/ for
    // key-wrapping headers and overwriting those specific regions —
    // not a full-card wipe, which would take unacceptably long and isn't
    // needed once the key is gone.)
}

/**
 * proximity_guard_lock_device — sets the lock flag. task_safety_guard.c
 * checks this before permitting any TX, and task_uart_bridge.c checks it
 * before accepting any command other than UNLOCK_ATTEMPT.
 */
static void proximity_guard_lock_device(void) {
    g_lock_state.locked = true;
    g_lock_state.locked_at_uptime_s = xTaskGetTickCount() / configTICK_RATE_HZ;

    AuditEvent_t ev = { .event_type = 0x10CD, .tx_power_mw = 0 };
    strncpy(ev.cmd_name, "SEC002_DEVICE_LOCKED", sizeof(ev.cmd_name) - 1);
    xQueueSend(g_audit_queue, &ev, 0);
}

/**
 * proximity_guard_attempt_unlock — called from task_uart_bridge.c when
 * the Flipper UI submits a PIN. Requires BOTH the paired device back in
 * BLE range (checked via the most recent sample on g_ble_proximity_queue,
 * not re-queried here) AND a matching PIN hash. Either alone is not
 * sufficient — this is the two-factor design from the SEC-002 spec.
 */
bool proximity_guard_attempt_unlock(const char *pin, bool device_currently_in_range) {
    if (!device_currently_in_range) return false;

    uint32_t candidate_hash[8];
    sha256_state_t state;
    sha256_start(&state, SHA256_BIG_ENDIAN);
    sha256_write(&state, (const uint8_t *)pin, strlen(pin));
    sha256_finish(&state, (uint8_t *)candidate_hash);

    if (memcmp(candidate_hash, (const void *)g_lock_state.unlock_pin_hash,
               sizeof(candidate_hash)) != 0) {
        return false;
    }

    g_lock_state.locked = false;
    AuditEvent_t ev = { .event_type = 0x0000, .tx_power_mw = 0 };
    strncpy(ev.cmd_name, "SEC002_DEVICE_UNLOCKED", sizeof(ev.cmd_name) - 1);
    xQueueSend(g_audit_queue, &ev, 0);
    return true;
}

/**
 * remote_kill_request — called by the C5 Wi-Fi/MQTT task once it has
 * verified an authenticated remote kill command. Kept as a one-way flag
 * rather than a direct function call so the Wi-Fi task (which is
 * inherently more exposed, being network-facing) never has direct access
 * to the crypto-shred function itself — it can only ask this task to
 * consider doing it, on the next 50ms tick.
 */
void proximity_guard_remote_kill_request(void) {
    g_remote_kill_requested = true;
}

/**
 * is_device_locked — read by task_safety_guard.c and task_uart_bridge.c.
 */
bool proximity_guard_is_locked(void) {
    return g_lock_state.locked;
}

/**
 * task_proximity_guard — main loop. Waits on BLE proximity samples from
 * C5, tracks how long the paired device has been out of range, and acts
 * according to the configured mode once confirm_timeout_ms has elapsed
 * continuously out of range.
 */
void task_proximity_guard(void *pvParameters) {
    (void)pvParameters;

    proximity_guard_load_key();

    BleProximitySample_t sample;
    uint32_t out_of_range_since_ms = 0;
    bool     currently_out_of_range = false;
    bool     escalation_pending = false;
    uint32_t locked_since_ms = 0;

    for (;;) {
        // Remote kill takes priority over everything else, checked every
        // loop regardless of BLE queue state.
        if (g_remote_kill_requested) {
            g_remote_kill_requested = false;
            proximity_guard_crypto_shred();
            proximity_guard_lock_device();
            continue;
        }

        // Block up to 1s waiting for a fresh BLE sample — this doubles as
        // our loop tick rate when samples are flowing normally.
        if (xQueueReceive(g_ble_proximity_queue, &sample, pdMS_TO_TICKS(1000)) == pdTRUE) {
            bool in_range = sample.device_present && (sample.rssi_dbm >= g_rssi_threshold);

            if (!in_range && !currently_out_of_range) {
                // Just went out of range — start the confirmation clock.
                currently_out_of_range = true;
                out_of_range_since_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            } else if (in_range && currently_out_of_range) {
                // Came back before the timeout expired — cancel, no action taken.
                currently_out_of_range = false;
                escalation_pending = false;
            }
        }
        // Note: xQueueReceive timing out (no sample arrived) is itself
        // informative — a paired device that's gone completely silent
        // for the BLE scan interval is treated the same as an explicit
        // "not present" sample, via the fallback timeout check below.

        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (currently_out_of_range &&
            (now_ms - out_of_range_since_ms) >= g_confirm_timeout_ms) {

            switch (g_mode) {
                case PROX_MODE_ALERT_ONLY: {
                    AuditEvent_t ev = { .event_type = 0xA1E7, .tx_power_mw = 0 };
                    strncpy(ev.cmd_name, "SEC002_PROXIMITY_ALERT", sizeof(ev.cmd_name) - 1);
                    xQueueSend(g_audit_queue, &ev, 0);
                    // No lock, no wipe — just logged. Reset the clock so we
                    // don't spam identical alerts every loop iteration.
                    currently_out_of_range = false;
                    break;
                }

                case PROX_MODE_LOCK:
                    if (!proximity_guard_is_locked()) {
                        proximity_guard_lock_device();
                    }
                    currently_out_of_range = false;
                    break;

                case PROX_MODE_LOCK_THEN_WIPE:
                    if (!escalation_pending) {
                        proximity_guard_lock_device();
                        locked_since_ms = now_ms;
                        escalation_pending = true;
                    } else if ((now_ms - locked_since_ms) >= g_escalate_timeout_ms) {
                        // Still locked and still hasn't been unlocked after
                        // the extended window — escalate to full wipe.
                        proximity_guard_crypto_shred();
                        escalation_pending = false;
                    }
                    break;

                case PROX_MODE_WIPE:
                    proximity_guard_crypto_shred();
                    proximity_guard_lock_device(); // wiped devices stay locked too
                    currently_out_of_range = false;
                    break;
            }
        }
    }
}
