/**
 * ESP32-C5-WROOM-1 RF Frontend — Neuro-Sniffer Pro v0.3.3.0
 * SDK: ESP-IDF v5.5+ MANDATORY for C5 (esp32c5 target, RISC-V 240MHz)
 * C5 vs C6: C5=dual-band 2.4+5GHz, 240MHz, #1 CSI. C6=single-band 2.4GHz, 160MHz.
 *
 * Build: idf.py set-target esp32c5 && idf.py build && idf.py flash
 * SPI slave from RP2350B: GPIO4(CS) GPIO5(CLK) GPIO6(MOSI) GPIO7(MISO)
 *
 * SPI SLAVE WIRED UP (2026-11-18, see ADR-018): previously this file never
 * initialised the SPI slave peripheral at all — RP2350B's route_cmd() was
 * writing commands into a bus nothing on this chip was listening to, and
 * ghost_tracker.c's analysis_task was formatting GHOST:{...} status lines
 * into a ring buffer (spi_buf) that nothing ever drained. spi_slave_task()
 * below closes that loop: it answers every transaction RP2350B initiates
 * with whatever Ghost Tracker reply is queued (or a generic idle ACK), and
 * logs whatever command text RP2350B sent. IMPORTANT — command execution
 * is NOT wired up yet: WIFI_SCAN_2G, DEAUTH, etc. are received and logged
 * here but don't do anything beyond that. Ghost Tracker itself runs
 * unconditionally regardless of any command (see app_main below), so it's
 * the only thing actually flowing end-to-end to the Flipper right now.
 * See docs/EXTERNAL_API_SPEC.md §6 for the full list of what's still open.
 */
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_slave.h"
#include <string.h>

static const char *TAG = "C5";
void ghost_tracker_init(void);
void ghost_set_gps(float lat, float lon);
bool ghost_tracker_get_reply(char *out, size_t maxlen);

// Pins match this file's own header comment above — the RP2350B-side CS
// pin (GPIO5 on RP2350B, see main.cpp/INTERNAL_API_CONTRACT.md) is a
// different numbering domain from these C5-side pins; each chip names
// its own GPIOs. SPI_XFER_LEN must match RP2350B's SPI_XFER_LEN exactly
// (fixed-length transactions — no separate "ready" line exists to
// negotiate a variable length with only these four wires).
#define PIN_CS    4
#define PIN_CLK   5
#define PIN_MOSI  6
#define PIN_MISO  7
#define SPI_XFER_LEN 256

static void spi_slave_task(void *p) {
    (void)p;
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_slave_interface_config_t slvcfg = {
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 3,
        .flags = 0,
    };
    esp_err_t init_r = spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    if (init_r != ESP_OK) {
        ESP_LOGE(TAG, "SPI slave init failed: %s — command path is dead, Ghost Tracker still runs standalone", esp_err_to_name(init_r));
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "SPI slave listening (CS=%d CLK=%d MOSI=%d MISO=%d, %d-byte transactions)",
             PIN_CS, PIN_CLK, PIN_MOSI, PIN_MISO, SPI_XFER_LEN);

    // DMA-capable buffers must be word-aligned.
    WORD_ALIGNED_ATTR static uint8_t rx[SPI_XFER_LEN];
    WORD_ALIGNED_ATTR static uint8_t tx[SPI_XFER_LEN];

    while (1) {
        memset(rx, 0, SPI_XFER_LEN);
        memset(tx, 0, SPI_XFER_LEN);

        // Fill this transaction's outgoing buffer with whatever's queued.
        // Today that's only Ghost Tracker status lines — see the file
        // header comment above for what's intentionally not here yet.
        char reply[SPI_XFER_LEN];
        if (ghost_tracker_get_reply(reply, sizeof(reply))) {
            strncpy((char *)tx, reply, SPI_XFER_LEN - 1);
        } else {
            strncpy((char *)tx, "ACK:IDLE\n", SPI_XFER_LEN - 1);
        }

        spi_slave_transaction_t t = {
            .length = SPI_XFER_LEN * 8,
            .tx_buffer = tx,
            .rx_buffer = rx,
        };
        // Blocks here until RP2350B clocks a transaction through on CS.
        esp_err_t r = spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
        if (r == ESP_OK && rx[0] != '\0') {
            ESP_LOGI(TAG, "RX cmd from RP2350B: %s", (char *)rx);
            // No command dispatch yet — see header comment. Logging only.
        }
    }
}

static void sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    (void)buf; (void)type;
    // Parse frame → send BSSID/RSSI/channel summary to RP2350B via SPI
    // Log full packet to /sd/captures/wifi/wardrive.pcap via RP2350B relay
}

static void ota_c5(void) {
    const char *url = "https://github.com/[your-user]/neuro-sniffer-pro"
                      "/releases/latest/download/esp32c5_firmware.bin";
    esp_http_client_config_t hc = { .url=url, .timeout_ms=30000 };
    esp_https_ota_config_t oc   = { .http_config=&hc };
    if (esp_https_ota(&oc)==ESP_OK) { ESP_LOGI(TAG,"OTA done"); esp_restart(); }
    else ESP_LOGE(TAG,"OTA failed");
}

void app_main(void) {
    ESP_LOGI(TAG,"Neuro-Sniffer Pro — ESP32-C5-WROOM-1 | IDF %s",esp_get_idf_version());
    esp_err_t r=nvs_flash_init();
    if(r==ESP_ERR_NVS_NO_FREE_PAGES||r==ESP_ERR_NVS_NEW_VERSION_FOUND){
        nvs_flash_erase(); r=nvs_flash_init();}
    ESP_ERROR_CHECK(r);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(sniffer_cb));
    ghost_tracker_init(); // Always start — passive, zero TX, always legal
    xTaskCreate(spi_slave_task, "spi_slave", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG,"C5 ready. Dual-band 2.4+5GHz + CSI ghost tracker active. SPI slave up.");
    while(1){vTaskDelay(pdMS_TO_TICKS(10));}
}
