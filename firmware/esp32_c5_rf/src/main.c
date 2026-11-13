/**
 * ESP32-C5-WROOM-1 RF Frontend — Neuro-Sniffer Pro v0.3.3.0
 * SDK: ESP-IDF v5.5+ MANDATORY for C5 (esp32c5 target, RISC-V 240MHz)
 * C5 vs C6: C5=dual-band 2.4+5GHz, 240MHz, #1 CSI. C6=single-band 2.4GHz, 160MHz.
 *
 * Build: idf.py set-target esp32c5 && idf.py build && idf.py flash
 * SPI slave from RP2350B: GPIO4(CS) GPIO5(CLK) GPIO6(MOSI) GPIO7(MISO)
 */
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "C5";
void ghost_tracker_init(void);
void ghost_set_gps(float lat, float lon);

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
    ESP_LOGI(TAG,"C5 ready. Dual-band 2.4+5GHz + CSI ghost tracker active.");
    while(1){vTaskDelay(pdMS_TO_TICKS(10));}
}
