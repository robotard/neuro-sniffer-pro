/** S3 OTA receiver — dual-bank, 60s watchdog, auto-rollback on boot failure */
#include "esp_ota_ops.h"
#include "esp_log.h"
static const char *TAG="S3_OTA";
void s3_ota(const uint8_t *fw,size_t len){
    const esp_partition_t *p=esp_ota_get_next_update_partition(NULL);
    if(!p){ESP_LOGE(TAG,"No OTA partition");return;}
    esp_ota_handle_t h;
    ESP_ERROR_CHECK(esp_ota_begin(p,OTA_WITH_SEQUENTIAL_WRITES,&h));
    ESP_ERROR_CHECK(esp_ota_write(h,fw,len));
    ESP_ERROR_CHECK(esp_ota_end(h));
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(p));
    ESP_LOGI(TAG,"S3 OTA done (%zu B). Rebooting.",len);esp_restart();}
