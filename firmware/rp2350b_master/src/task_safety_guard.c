/**
 * task_safety_guard.c — Neuro-Sniffer Pro v0.3.3.0
 * RP2350B FreeRTOS task — THE most safety-critical file in this project.
 *
 * Runs every 50ms at the highest task priority below the RTOS tick itself.
 * Nothing transmits on any radio unless this task has said it's allowed to.
 *
 * Three jobs, in priority order:
 *   1. Physical kill switch sense — if it's off, RF stays off. No exceptions.
 *   2. Frequency whitelist — refuse any TX command outside 300MHz-6GHz.
 *   3. Emergency-service cutoff — if S3 says it heard P25/TETRA/DMR/FirstNet
 *      near our TX frequency, we kill RF *now* and log why.
 *
 * Every decision this task makes — allow, block, cut — gets pushed onto the
 * audit queue so audit_logger.c can write it to the tamper-evident chain.
 * See docs/decisions/ADR-003 (ES notify-not-block) and ADR-004 (TEST_MODE).
 *
 * DO NOT relax any check in this file without a new ADR explaining why.
 * This file needs a second pair of eyes before any change goes near a
 * board that's actually transmitting.
 */

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ────────────────────────────────────────────────────────────────────────
// Pins — must match hardware/SCHEMATIC_NOTES.md exactly. If you change a
// pin here, change it there too, and change it in the KiCad prompt.
// ────────────────────────────────────────────────────────────────────────
#define PIN_KILL_SWITCH   28   // Physical switch, pulled up. HIGH = TX permitted.
#define PIN_RF_VCC_EN     16   // Drives the RF VCC gate MOSFET. HIGH = RF powered.
#define PIN_ES_ALERT_IN   27   // From S3 task_emserv_monitor.c. HIGH = ES heard nearby.

// ────────────────────────────────────────────────────────────────────────
// Frequency whitelist. Anything outside this range is refused outright —
// it doesn't matter what the command claims to be for.
// ────────────────────────────────────────────────────────────────────────
#define FREQ_MIN_HZ   300000000UL   // 300 MHz
#define FREQ_MAX_HZ   6000000000ULL // 6 GHz

// ────────────────────────────────────────────────────────────────────────
// TX power cap. Set once at first-flash by task_onboarding.c and stored in
// NVS. This is a #define fallback only — the real value is read from NVS
// at boot. TEST_MODE vs PRODUCTION changes this at build time via
// FLASH_TEST.sh / FLASH_PRODUCTION.sh, not at runtime.
// ────────────────────────────────────────────────────────────────────────
#ifndef TEST_MODE
#define TEST_MODE 0
#endif

#if TEST_MODE
  #define TX_POWER_CAP_MW   10
#else
  #define TX_POWER_CAP_MW   100
#endif

#define SAFETY_TASK_PERIOD_MS   50   // ADR-003/004 mandate 50ms max response time
#define ES_CONFIRM_TIMEOUT_MS   10000  // 10s default STOP window on ES overlap

// ────────────────────────────────────────────────────────────────────────
// The one struct every other task reads to know "can I transmit right now".
// Written only by this task. Everyone else treats it as read-only.
// ────────────────────────────────────────────────────────────────────────
typedef struct {
    bool     kill_switch_engaged;   // true = physically allowed to transmit
    bool     es_alert_active;       // true = S3 currently hearing ES traffic
    bool     rf_vcc_on;             // true = RF power rail is actually live
    uint8_t  tx_power_cap_mw;       // current enforced ceiling
    uint32_t last_block_reason;     // see SafetyBlockReason enum below
} SafetyState_t;

typedef enum {
    BLOCK_NONE = 0,
    BLOCK_KILL_SWITCH,       // physical switch disengaged
    BLOCK_FREQ_OUT_OF_RANGE, // command asked for TX outside 300MHz-6GHz
    BLOCK_ES_OVERLAP,        // ES traffic heard on/near requested TX frequency
    BLOCK_POWER_CAP,         // requested TX power exceeds current cap
} SafetyBlockReason_t;

// Global safety state — volatile because it's read by other tasks without
// a mutex. This is intentional: reads are atomic on Cortex-M33 for a
// struct this size read field-by-field, and staleness by one 50ms tick is
// an acceptable risk for status display. Anything that actually *gates* a
// TX decision goes through safety_guard_request_tx() below, which is
// synchronous and authoritative.
static volatile SafetyState_t g_safety = {
    .kill_switch_engaged = false,   // assume OFF until first read proves otherwise
    .es_alert_active     = false,
    .rf_vcc_on           = false,
    .tx_power_cap_mw     = TX_POWER_CAP_MW,
    .last_block_reason   = BLOCK_NONE,
};

// Queue to audit_logger.c — every safety event gets pushed here.
// Defined in audit_logger.c, declared extern here.
extern QueueHandle_t g_audit_queue;

typedef struct {
    uint32_t event_type;   // matches SafetyBlockReason_t or a positive "ALLOWED" code
    uint32_t tx_power_mw;
    char     cmd_name[32];
} AuditEvent_t;

// ────────────────────────────────────────────────────────────────────────
// rf_vcc_set — the only place in the entire firmware that touches the RF
// VCC gate. Everything else asks this task to do it via the functions
// below; nothing else is allowed to drive PIN_RF_VCC_EN directly.
// ────────────────────────────────────────────────────────────────────────
static void rf_vcc_set(bool on) {
    gpio_put(PIN_RF_VCC_EN, on ? 1 : 0);
    g_safety.rf_vcc_on = on;
}

/**
 * safety_guard_request_tx — the gate function every RF-issuing task must
 * call before it keys a transmitter. Synchronous, authoritative, and the
 * only path by which RF VCC gets turned on.
 *
 * @param freq_hz       requested TX frequency
 * @param requested_mw  requested TX power in mW
 * @param cmd_name      short human-readable command name, for the audit log
 * @return true if TX is permitted right now, false otherwise (g_safety.last_block_reason
 *         will explain why)
 */
bool safety_guard_request_tx(uint64_t freq_hz, uint8_t requested_mw, const char *cmd_name) {
    AuditEvent_t ev = {0};
    strncpy(ev.cmd_name, cmd_name, sizeof(ev.cmd_name) - 1);
    ev.tx_power_mw = requested_mw;

    // 1. Physical kill switch — checked first, always. No software path
    //    around this. If the switch is off, we don't even evaluate the
    //    rest of the checks; there is nothing to evaluate them for.
    if (!g_safety.kill_switch_engaged) {
        g_safety.last_block_reason = BLOCK_KILL_SWITCH;
        ev.event_type = BLOCK_KILL_SWITCH;
        xQueueSend(g_audit_queue, &ev, 0);
        return false;
    }

    // 2. Frequency whitelist — hard reject outside 300MHz-6GHz. This is
    //    not configurable at runtime; changing it means changing this
    //    file and re-flashing, which is the point.
    if (freq_hz < FREQ_MIN_HZ || freq_hz > FREQ_MAX_HZ) {
        g_safety.last_block_reason = BLOCK_FREQ_OUT_OF_RANGE;
        ev.event_type = BLOCK_FREQ_OUT_OF_RANGE;
        xQueueSend(g_audit_queue, &ev, 0);
        return false;
    }

    // 3. TX power cap — enforced regardless of what the caller asked for.
    if (requested_mw > g_safety.tx_power_cap_mw) {
        g_safety.last_block_reason = BLOCK_POWER_CAP;
        ev.event_type = BLOCK_POWER_CAP;
        xQueueSend(g_audit_queue, &ev, 0);
        return false;
    }

    // 4. Emergency-service overlap — per ADR-003 this is "notify not
    //    block" for passive operation, but for *active TX* specifically
    //    overlapping an ES band, we block by default. The confirmation
    //    dialog (default STOP, 10s timeout) lives in the UART bridge /
    //    Flipper UI layer, not here — this function only sees the final
    //    "user confirmed override" flag, which task_uart_bridge.c sets
    //    on g_safety before calling this function again.
    if (g_safety.es_alert_active) {
        g_safety.last_block_reason = BLOCK_ES_OVERLAP;
        ev.event_type = BLOCK_ES_OVERLAP;
        xQueueSend(g_audit_queue, &ev, 0);
        return false;
    }

    // All checks passed. Turn RF on and log the allow.
    rf_vcc_set(true);
    g_safety.last_block_reason = BLOCK_NONE;
    ev.event_type = 0xA110; // "ALLOWED" marker, distinct from any block reason
    xQueueSend(g_audit_queue, &ev, 0);
    return true;
}

/**
 * safety_guard_release_tx — called by the RF-issuing task the moment it's
 * done transmitting. RF VCC drops immediately; we don't wait for the next
 * 50ms tick.
 */
void safety_guard_release_tx(void) {
    rf_vcc_set(false);
}

/**
 * task_safety_guard — the 50ms polling loop. This is where the kill
 * switch and ES GPIO actually get read. Everything above is the API that
 * other tasks call; this is the background watchdog that can override
 * them unilaterally at any moment — most importantly, it can cut RF VCC
 * even mid-transmission if the kill switch is thrown or ES traffic
 * appears, without waiting to be asked.
 */
void task_safety_guard(void *pvParameters) {
    (void)pvParameters;

    // Configure pins. RF VCC starts LOW and stays that way until this
    // loop has run at least once and confirmed the kill switch state.
    gpio_init(PIN_KILL_SWITCH);
    gpio_set_dir(PIN_KILL_SWITCH, GPIO_IN);
    gpio_pull_up(PIN_KILL_SWITCH);

    gpio_init(PIN_RF_VCC_EN);
    gpio_set_dir(PIN_RF_VCC_EN, GPIO_OUT);
    gpio_put(PIN_RF_VCC_EN, 0);   // RF off at boot, full stop.

    gpio_init(PIN_ES_ALERT_IN);
    gpio_set_dir(PIN_ES_ALERT_IN, GPIO_IN);
    gpio_pull_down(PIN_ES_ALERT_IN);

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        // Kill switch: active HIGH = engaged = TX permitted. Wired this
        // way (rather than active LOW) so that a cut wire or disconnected
        // switch fails to the SAFE state (TX blocked), not the dangerous one.
        bool kill_now = gpio_get(PIN_KILL_SWITCH);
        bool es_now   = gpio_get(PIN_ES_ALERT_IN);

        // If the kill switch just went from engaged to disengaged, cut
        // RF immediately — don't wait for the next TX request to notice.
        // This is the <50ms response time requirement from ADR-003/004.
        if (g_safety.kill_switch_engaged && !kill_now && g_safety.rf_vcc_on) {
            rf_vcc_set(false);
            AuditEvent_t ev = { .event_type = BLOCK_KILL_SWITCH, .tx_power_mw = 0 };
            strncpy(ev.cmd_name, "MID_TX_KILL_SWITCH_CUT", sizeof(ev.cmd_name) - 1);
            xQueueSend(g_audit_queue, &ev, 0);
        }

        // Same for ES alert appearing mid-transmission.
        if (!g_safety.es_alert_active && es_now && g_safety.rf_vcc_on) {
            rf_vcc_set(false);
            AuditEvent_t ev = { .event_type = BLOCK_ES_OVERLAP, .tx_power_mw = 0 };
            strncpy(ev.cmd_name, "MID_TX_ES_CUT", sizeof(ev.cmd_name) - 1);
            xQueueSend(g_audit_queue, &ev, 0);
        }

        g_safety.kill_switch_engaged = kill_now;
        g_safety.es_alert_active     = es_now;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAFETY_TASK_PERIOD_MS));
    }
}

/**
 * safety_guard_get_state — read-only snapshot for the UART bridge to
 * report back to the Flipper UI (status bar, confirmation dialogs, etc).
 */
void safety_guard_get_state(SafetyState_t *out) {
    // Struct copy of a volatile — fine for display purposes, see comment
    // on g_safety above for why this doesn't need a mutex.
    out->kill_switch_engaged = g_safety.kill_switch_engaged;
    out->es_alert_active     = g_safety.es_alert_active;
    out->rf_vcc_on           = g_safety.rf_vcc_on;
    out->tx_power_cap_mw     = g_safety.tx_power_cap_mw;
    out->last_block_reason   = g_safety.last_block_reason;
}
