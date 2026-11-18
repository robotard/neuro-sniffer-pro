/**
 * RP2350B Master Controller — Neuro-Sniffer Pro v0.3.3.0
 * Roles: Flipper bridge | safety guardian | OTA orchestrator | GPS | OLED | BadUSB
 *
 * Pin map: GPIO0/1=Flipper UART | GPIO2-6=SPI(C5,S3) | GPIO8/9=GPS UART
 *          GPIO10/11=I2C OLED | GPIO14-17=SPI microSD | GPIO20=C5 heartbeat
 *          GPIO21=S3 RST | GPIO22=C5 RST | GPIO27=ES alert in | GPIO28=kill switch
 *          GPIO16=RF VCC enable (owned exclusively by task_safety_guard.c)
 *
 * SAFETY FIX (2026-11-14): this file used to carry its own task_safety()
 * with a hard `kill && !emergency` AND-block and NO override path — a
 * second, competing copy of the safety gate that already exists properly
 * in task_safety_guard.c (kill-switch + frequency whitelist + power cap +
 * ADR-003 notify-not-block ES handling, all with audit logging). Two
 * tasks were also both initialising and polling the same GPIO28/GPIO27
 * pins into two different global structs — a race, not just duplication.
 * The duplicate has been deleted. This file now starts the one real
 * task_safety_guard() task and reads its state via safety_guard_get_state()
 * for UART status reporting and as a cheap pre-filter before forwarding a
 * command over SPI.
 *
 * TODO (tracked, not done here — flagging rather than silently skipping):
 * per ADR-017 the authoritative TX decision belongs on the control plane —
 * C5/S3 should call safety_guard_request_tx() (or an SPI-relayed
 * equivalent) themselves immediately before keying a transmitter, not
 * rely on this file's pre-filter below. That control-plane protocol, and
 * the task_uart_bridge.c override-confirmation flow task_safety_guard.c's
 * own comments refer to, are not implemented yet anywhere in this tree.
 * There is also no shared header (e.g. include/ctrl_role.h) exposing
 * SafetyState_t and the safety_guard_* prototypes — the extern block
 * below is a stopgap that must stay byte-for-byte in sync with
 * task_safety_guard.c's struct layout. Move this to a proper header
 * before this goes near a board that's actually transmitting, per the
 * "second pair of eyes" note at the top of task_safety_guard.c.
 */
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define PIN_C5_HB  20
#define PIN_C5_RST 22
#define PIN_S3_RST 21
#define PIN_C5_CS   5
#define PIN_S3_CS   6

// ─────────────────────────────────────────────────────────────────────────
// Safety guard API (task_safety_guard.c). Stopgap extern block — see TODO
// above. This struct layout MUST match task_safety_guard.c's SafetyState_t
// exactly; there is no shared header yet.
// ─────────────────────────────────────────────────────────────────────────
typedef struct {
    bool     kill_switch_engaged;   // true = physically allowed to transmit
    bool     es_alert_active;       // true = S3 currently hearing ES traffic
    bool     rf_vcc_on;             // true = RF power rail is actually live
    uint8_t  tx_power_cap_mw;       // current enforced ceiling
    uint32_t last_block_reason;     // SafetyBlockReason_t from task_safety_guard.c
} SafetyState_t;

extern void task_safety_guard(void *pvParameters);
extern void safety_guard_get_state(SafetyState_t *out);
extern bool safety_guard_request_tx(uint64_t freq_hz, uint8_t requested_mw, const char *cmd_name);
extern void safety_guard_release_tx(void);

static void spi_send(uint8_t cs, const char *cmd) {
    // Write-only. Still used for S3 (PIN_S3_CS) below — S3's main.cpp has
    // no SPI slave listener implemented and no documented GPIO assignment
    // for one yet (see ADR-018), so reading back from that CS line would
    // just clock floating/garbage data. Flagged, not silently worked
    // around with invented pins.
    gpio_put(cs, 0);
    spi_write_blocking(spi0, (const uint8_t *)cmd, strlen(cmd));
    gpio_put(cs, 1);
}

// SPI_XFER_LEN (2026-11-18, ADR-018): C5's SPI slave task now expects an
// exact SPI_XFER_LEN-byte full-duplex transaction every time (no separate
// "ready" line exists on this 4-wire bus to negotiate a variable length).
// This value MUST match firmware/esp32_c5_rf/src/main.c's SPI_XFER_LEN.
#define SPI_XFER_LEN 256

// Full-duplex: write the command (zero-padded to SPI_XFER_LEN) while
// simultaneously reading back whatever C5's spi_slave_task queued for us
// in the same transaction. reply_out is NUL-terminated on return; pass
// NULL to ignore the reply entirely.
static void spi_transfer_c5(const char *cmd, char *reply_out, size_t reply_cap) {
    uint8_t tx[SPI_XFER_LEN] = {0};
    uint8_t rx[SPI_XFER_LEN] = {0};
    size_t n = strlen(cmd);
    if (n >= SPI_XFER_LEN) n = SPI_XFER_LEN - 1;
    memcpy(tx, cmd, n);

    gpio_put(PIN_C5_CS, 0);
    spi_write_read_blocking(spi0, tx, rx, SPI_XFER_LEN);
    gpio_put(PIN_C5_CS, 1);

    if (reply_out && reply_cap) {
        size_t m = strnlen((char *)rx, SPI_XFER_LEN);
        if (m >= reply_cap) m = reply_cap - 1;
        memcpy(reply_out, rx, m);
        reply_out[m] = '\0';
    }
}

static void route_cmd(const char *cmd) {
    static const char *tx_cmds[] = {
        "DEAUTH","EVIL_TWIN","BEACON_FLOOD","WIFI_JAM","HANDSHAKE_CAP",
        "BLE_FLOOD","BLE_SPOOF","BLE_FUZZ","SUBGHZ_REPLAY","SUBGHZ_JAM",
        "NFC_WRITE","NFC_CLONE","DUCKY_START", NULL
    };

    // Cheap pre-filter only — NOT the authoritative gate. See TODO above.
    SafetyState_t safety;
    safety_guard_get_state(&safety);

    for (int i = 0; tx_cmds[i]; i++) {
        if (strncmp(cmd, tx_cmds[i], strlen(tx_cmds[i])) == 0 &&
            !(safety.kill_switch_engaged && !safety.es_alert_active)) {
            uart_puts(uart0, "ERROR:SAFETY_LOCKED\r\n"); return;
        }
    }
    if (strncmp(cmd,"WIFI_",5)==0||strncmp(cmd,"BLE_",4)==0||
        strncmp(cmd,"ZIGBEE_",7)==0||strncmp(cmd,"OTA_",4)==0||
        strncmp(cmd,"GHOST_",6)==0||strncmp(cmd,"WARDRIVE",8)==0) {
        // Full-duplex now (ADR-018): C5 actually has an SPI slave task as
        // of this change, and already feeds real Ghost Tracker status
        // lines back through it. Forward whatever comes back to the
        // Flipper, except the generic idle ACK (nothing worth repeating).
        char reply[SPI_XFER_LEN];
        spi_transfer_c5(cmd, reply, sizeof(reply));
        if (reply[0] && strncmp(reply, "ACK:IDLE", 8) != 0) {
            uart_puts(uart0, reply);
            size_t rl = strlen(reply);
            if (rl == 0 || reply[rl-1] != '\n') uart_puts(uart0, "\r\n");
        }
    }
    else if (strncmp(cmd,"SUBGHZ_",7)==0||strncmp(cmd,"NFC_",4)==0||
             strncmp(cmd,"AI_",3)==0||strncmp(cmd,"MODEL_",6)==0)
        spi_send(PIN_S3_CS, cmd);
    else if (strcmp(cmd,"SAFETY_STATUS")==0) {
        char b[128];
        snprintf(b,sizeof(b),
            "SAFETY:{\"kill\":%d,\"es_alert\":%d,\"rf_vcc\":%d,\"pwr_cap_mw\":%d,\"block_reason\":%lu}\r\n",
            safety.kill_switch_engaged, safety.es_alert_active, safety.rf_vcc_on,
            safety.tx_power_cap_mw, (unsigned long)safety.last_block_reason);
        uart_puts(uart0,b);
    } else if (strcmp(cmd,"REBOOT")==0) watchdog_reboot(0,0,100);
}

static void task_flipper(void *p) {
    (void)p;
    char buf[256]; uint8_t idx=0;
    bool es_was_active = false;
    while (1) {
        if (uart_is_readable(uart0)) {
            char c=uart_getc(uart0);
            if (c=='\n'||idx>=254){buf[idx]='\0';if(idx>0)route_cmd(buf);idx=0;}
            else if(c!='\r') buf[idx++]=c;
        }
        // ADR-003 notify-not-block: surface ES proximity to the operator on
        // every rising edge, same behaviour the old duplicate task had —
        // just reading the one real safety state now instead of a second,
        // racing copy of it.
        SafetyState_t safety;
        safety_guard_get_state(&safety);
        if (safety.es_alert_active && !es_was_active) {
            uart_puts(uart0,"ALERT:EMERGENCY_SHIELD_ACTIVE\r\n");
        }
        es_was_active = safety.es_alert_active;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

int main(void) {
    stdio_init_all();
    uart_init(uart0,115200); gpio_set_function(0,GPIO_FUNC_UART); gpio_set_function(1,GPIO_FUNC_UART);
    uart_init(uart1,9600);   gpio_set_function(8,GPIO_FUNC_UART); gpio_set_function(9,GPIO_FUNC_UART);
    spi_init(spi0,10000000);
    gpio_set_function(2,GPIO_FUNC_SPI); gpio_set_function(3,GPIO_FUNC_SPI); gpio_set_function(4,GPIO_FUNC_SPI);
    int cs_pins[] = {PIN_C5_CS,PIN_S3_CS,PIN_C5_RST,PIN_S3_RST};
    for (int i=0;i<4;i++){gpio_init(cs_pins[i]);gpio_set_dir(cs_pins[i],GPIO_OUT);gpio_put(cs_pins[i],1);}
    i2c_init(i2c0,400000); gpio_set_function(10,GPIO_FUNC_I2C); gpio_set_function(11,GPIO_FUNC_I2C);
    gpio_init(PIN_C5_HB); gpio_set_dir(PIN_C5_HB,GPIO_IN);
    // Kill switch (GPIO28) / ES-alert (GPIO27) / RF-VCC (GPIO16) pins are
    // owned and initialised exclusively inside task_safety_guard() — not
    // touched here, so there is exactly one owner of each pin.
    watchdog_enable(5000,true);
    uart_puts(uart0,"NEURO:READY\r\n");
    // Highest application task priority — below only the RTOS tick itself,
    // per the priority contract documented at the top of task_safety_guard.c.
    xTaskCreate(task_safety_guard,"safety_guard",512,NULL,4,NULL);
    xTaskCreate(task_flipper,"flipper",1024,NULL,2,NULL);
    vTaskStartScheduler();
    while(1){watchdog_update();}
}
