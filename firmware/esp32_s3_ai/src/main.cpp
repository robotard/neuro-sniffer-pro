/**
 * ESP32-S3 AI Co-processor — Neuro-Sniffer Pro v0.3.3.0
 * Core 0: TFLite inference + emergency detection (GPIO4→RP2350B GPIO27)
 * Core 1: SX1262 Sub-GHz + PN532 NFC + SPI slave to RP2350B
 * Emergency: GPIO4 HIGH on P25/TETRA/DMR/FirstNet detect → RP2350B kills all TX <50ms
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include <string.h>

static const char *TAG="S3";
#define PIN_EMERG 4
#define ARENA_KB  96

extern const uint8_t tflite_model[] asm("_binary_model_tflite_start");
static uint8_t arena[ARENA_KB*1024];
static tflite::MicroInterpreter *interp=nullptr;

static void tflite_init(void){
    const tflite::Model *m=tflite::GetModel(tflite_model);
    static tflite::MicroMutableOpResolver<6> res;
    res.AddConv2D();res.AddDepthwiseConv2D();res.AddReshape();
    res.AddSoftmax();res.AddFullyConnected();res.AddMaxPool2D();
    static tflite::MicroInterpreter si(m,res,arena,sizeof(arena));
    interp=&si;interp->AllocateTensors();
    ESP_LOGI(TAG,"TFLite ready. Arena: %zu B",interp->arena_used_bytes());}

// Returns: 0=normal 1=garage 2=lock 3=lora 4=anomaly -1=EMERGENCY
int classify(const float *iq,int len){
    if(!interp)return 0;
    TfLiteTensor *in=interp->input(0);
    memcpy(in->data.f,iq,len*sizeof(float));
    interp->Invoke();
    TfLiteTensor *out=interp->output(0);
    float *sc=out->data.f;int best=0;
    for(int i=1;i<out->dims->data[1];i++)if(sc[i]>sc[best])best=i;
    if(best==5&&sc[5]>0.92f){gpio_set_level(PIN_EMERG,1);
        ESP_LOGE(TAG,"EMERGENCY — GPIO4 HIGH → TX killed");return -1;}
    gpio_set_level(PIN_EMERG,0);return best;}

extern "C" void app_main(void){
    gpio_config_t io={.pin_bit_mask=(1ULL<<PIN_EMERG),.mode=GPIO_MODE_OUTPUT};
    gpio_config(&io);gpio_set_level(PIN_EMERG,0);
    tflite_init();
    ESP_LOGI(TAG,"S3 ready. Core0:AI | Core1:Sub-GHz+NFC");
    while(1){vTaskDelay(pdMS_TO_TICKS(10));}}
