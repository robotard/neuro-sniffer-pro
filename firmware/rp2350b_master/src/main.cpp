/**
 * RP2350B Master Controller — Neuro-Sniffer Pro v0.3.3.0
 * Roles: Flipper bridge | safety guardian | OTA orchestrator | GPS | OLED | BadUSB
 *
 * Pin map: GPIO0/1=Flipper UART | GPIO2-6=SPI(C5,S3) | GPIO8/9=GPS UART
 *          GPIO10/11=I2C OLED | GPIO14-17=SPI microSD | GPIO20=C5 heartbeat
 *          GPIO21=S3 RST | GPIO22=C5 RST | GPIO27=emergency in | GPIO28=kill switch
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

#define PIN_KILL   28
#define PIN_EMERG  27
#define PIN_C5_HB  20
#define PIN_C5_RST 22
#define PIN_S3_RST 21
#define PIN_C5_CS   5
#define PIN_S3_CS   6

typedef struct { bool kill, emergency, tx_ok; uint8_t pwr_mw; } Safety_t;
static Safety_t S = { .pwr_mw = 10 };

static void spi_send(uint8_t cs, const char *cmd) {
    gpio_put(cs, 0);
    spi_write_blocking(spi0, (const uint8_t *)cmd, strlen(cmd));
    gpio_put(cs, 1);
}

static void task_safety(void *p) {
    (void)p;
    while (1) {
        S.kill      = gpio_get(PIN_KILL);
        S.emergency = gpio_get(PIN_EMERG);
        S.tx_ok     = S.kill && !S.emergency;
        if (!S.tx_ok) { spi_send(PIN_C5_CS,"TX_DISABLE"); spi_send(PIN_S3_CS,"TX_DISABLE"); }
        if (S.emergency) uart_puts(uart0,"ALERT:EMERGENCY_SHIELD_ACTIVE\r\n");
        watchdog_update();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void route_cmd(const char *cmd) {
    static const char *tx_cmds[] = {
        "DEAUTH","EVIL_TWIN","BEACON_FLOOD","WIFI_JAM","HANDSHAKE_CAP",
        "BLE_FLOOD","BLE_SPOOF","BLE_FUZZ","SUBGHZ_REPLAY","SUBGHZ_JAM",
        "NFC_WRITE","NFC_CLONE","DUCKY_START", NULL
    };
    for (int i = 0; tx_cmds[i]; i++) {
        if (strncmp(cmd, tx_cmds[i], strlen(tx_cmds[i])) == 0 && !S.tx_ok) {
            uart_puts(uart0, "ERROR:SAFETY_LOCKED\r\n"); return;
        }
    }
    if (strncmp(cmd,"WIFI_",5)==0||strncmp(cmd,"BLE_",4)==0||
        strncmp(cmd,"ZIGBEE_",7)==0||strncmp(cmd,"OTA_",4)==0||
        strncmp(cmd,"GHOST_",6)==0||strncmp(cmd,"WARDRIVE",8)==0)
        spi_send(PIN_C5_CS, cmd);
    else if (strncmp(cmd,"SUBGHZ_",7)==0||strncmp(cmd,"NFC_",4)==0||
             strncmp(cmd,"AI_",3)==0||strncmp(cmd,"MODEL_",6)==0)
        spi_send(PIN_S3_CS, cmd);
    else if (strcmp(cmd,"SAFETY_STATUS")==0) {
        char b[96];
        snprintf(b,sizeof(b),"SAFETY:{\"kill\":%d,\"emerg\":%d,\"tx\":%d,\"pwr\":%d}\r\n",
            S.kill,S.emergency,S.tx_ok,S.pwr_mw);
        uart_puts(uart0,b);
    } else if (strcmp(cmd,"REBOOT")==0) watchdog_reboot(0,0,100);
}

static void task_flipper(void *p) {
    (void)p;
    char buf[256]; uint8_t idx=0;
    while (1) {
        if (uart_is_readable(uart0)) {
            char c=uart_getc(uart0);
            if (c=='\n'||idx>=254){buf[idx]='\0';if(idx>0)route_cmd(buf);idx=0;}
            else if(c!='\r') buf[idx++]=c;
        }
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
    gpio_init(PIN_KILL);  gpio_set_dir(PIN_KILL, GPIO_IN); gpio_pull_up(PIN_KILL);
    gpio_init(PIN_EMERG); gpio_set_dir(PIN_EMERG,GPIO_IN); gpio_pull_down(PIN_EMERG);
    gpio_init(PIN_C5_HB); gpio_set_dir(PIN_C5_HB,GPIO_IN);
    watchdog_enable(5000,true);
    uart_puts(uart0,"NEURO:READY\r\n");
    xTaskCreate(task_safety,"safety",512,NULL,3,NULL);
    xTaskCreate(task_flipper,"flipper",1024,NULL,2,NULL);
    vTaskStartScheduler();
    while(1){watchdog_update();}
}
