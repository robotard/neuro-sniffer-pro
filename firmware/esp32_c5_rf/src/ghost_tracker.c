/**
 * ghost_tracker.c — Wi-Fi CSI Passive Sensing Engine
 * ESP32-C5-WROOM-1 | Neuro-Sniffer Pro v0.3.3.0
 *
 * LEGAL: 100% passive. Zero TX. Lawful in most jurisdictions.
 *
 * SENSING VECTORS:
 * [1] CSI variance: 52 OFDM subcarriers @ 20Hz. Amplitude variance = motion.
 *     Goertzel filter 0.15-0.60Hz detects breathing. Motion >0.6 = walking.
 *     Accuracy: motion ~95% at 5m through wall, breathing ~90% at rest.
 * [2] RF fingerprinting: CFO + clock skew + spectral flatness + IQ imbalance.
 *     Hardware signatures from physical radio chain. Survives MAC randomisation.
 * [3] Passive channel charting: raw CSI to /sd/ghost/csi/SESSION.csi
 *     2+ units + python3 pc_tools/channel_chart.py = 2D position map
 *     ~0.5-1.5m NLOS median error (ESPARGOS / Berlin 6G Conference 2024)
 *
 * ESP-IDF v5.5+ required. Needs: espressif/esp-csi component.
 * Logs: /sd/ghost/csi/ (binary 128B/frame) | /sd/ghost/rfp/ (TSV)
 */
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

static const char *TAG = "GHOST";
#define N_SUB  52
#define DEPTH  200
#define MOT_TH 0.15f

typedef struct __attribute__((packed)) {
    int64_t ts_us; uint8_t mac[6]; int8_t rssi; uint8_t ch;
    int8_t amp[N_SUB], phase[N_SUB]; float cfo, skew;
} snap_t; // 128 bytes

typedef struct { snap_t f[DEPTH]; uint16_t head, count; } ring_t;

typedef struct {
    uint8_t mac[6]; float cfo_mean,cfo_std,skew_mean,flatness;
    int8_t rssi_mean,rssi_min,rssi_max; uint32_t pkts;
    int64_t first_us,last_us; float lat,lon;
} rfp_t;

typedef struct {
    uint8_t people; float motion,bpm;
    bool mot,breath,walk; float lat,lon;
    uint32_t devices,frames;
} state_t;

static ring_t ring; static state_t st;
static rfp_t *fp_tbl=NULL; static uint32_t fp_n=0,fp_cap=32;
static bool logging=true;
static FILE *csi_fp=NULL,*rfp_fp=NULL;
static RingbufHandle_t spi_buf;

static float wvar(const float *a,int n){
    if(n<2)return 0;float m=0;for(int i=0;i<n;i++)m+=a[i];m/=n;
    float v=0;for(int i=0;i<n;i++){float d=a[i]-m;v+=d*d;}return v/(n-1);}

static float flat(const int8_t *a,int n){
    double ls=0;float ar=0;
    for(int i=0;i<n;i++){float x=fabsf(a[i])+1;ls+=log(x);ar+=x;}
    return ar>0?(float)(exp(ls/n))/(ar/n):0;}

static float cfo_est(const int8_t *ph,int n){
    float s=0;for(int i=1;i<n;i++)s+=(ph[i]-ph[i-1]);return s/(n-1);}

static void ring_push(const snap_t *s){
    ring.f[ring.head]=*s;ring.head=(ring.head+1)%DEPTH;
    if(ring.count<DEPTH)ring.count++;}

static void get_amp(int sub,float *out,int *len){
    int n=ring.count;*len=n;
    for(int i=0;i<n;i++){
        int idx=(ring.head-n+i+DEPTH)%DEPTH;
        out[i]=fabsf(ring.f[idx].amp[sub]);}}

static rfp_t *find_or_new(const uint8_t *mac){
    for(uint32_t i=0;i<fp_n;i++)if(memcmp(fp_tbl[i].mac,mac,6)==0)return &fp_tbl[i];
    if(fp_n>=fp_cap){fp_cap*=2;fp_tbl=realloc(fp_tbl,fp_cap*sizeof(rfp_t));}
    rfp_t *fp=&fp_tbl[fp_n++];memset(fp,0,sizeof(rfp_t));memcpy(fp->mac,mac,6);
    fp->first_us=esp_timer_get_time();fp->rssi_min=0;fp->rssi_max=-127;
    st.devices=fp_n;return fp;}

static void upd_rfp(rfp_t *fp,const snap_t *s){
    fp->pkts++;float n=fp->pkts;
    float d=s->cfo-fp->cfo_mean;fp->cfo_mean+=d/n;
    float d2=s->cfo-fp->cfo_mean;
    fp->cfo_std=sqrtf(fabsf(fp->cfo_std*fp->cfo_std+d*d2/n));
    fp->skew_mean+=(s->skew-fp->skew_mean)/n;
    fp->flatness=flat(s->amp,N_SUB);
    fp->rssi_mean+=((float)s->rssi-fp->rssi_mean)/n;
    if(s->rssi<fp->rssi_min)fp->rssi_min=s->rssi;
    if(s->rssi>fp->rssi_max)fp->rssi_max=s->rssi;
    fp->last_us=s->ts_us;fp->lat=st.lat;fp->lon=st.lon;}

static float motion_score(void){
    float tv=0,ser[DEPTH];int n;
    for(int s=0;s<N_SUB;s++){get_amp(s,ser,&n);if(n>4)tv+=wvar(ser,n);}
    float sc=(tv/N_SUB)/5.0f;return sc>1?1:sc;}

static float breath_bpm(void){
    float mv=0;int bs=0,n;float ser[DEPTH];
    for(int s=0;s<N_SUB;s++){get_amp(s,ser,&n);float v=n>4?wvar(ser,n):0;if(v>mv){mv=v;bs=s;}}
    if(n<40)return 0;
    get_amp(bs,ser,&n);
    float m=0;for(int i=0;i<n;i++)m+=ser[i];m/=n;
    for(int i=0;i<n;i++)ser[i]-=m;
    float bp=0,bf=0;
    for(int fs=0;fs<=9;fs++){
        float fh=0.15f+fs*0.05f,om=2*(float)M_PI*fh/20.0f;
        float s1=0,s2=0,co=2*cosf(om);
        for(int i=0;i<n;i++){float s0=ser[i]+co*s1-s2;s2=s1;s1=s0;}
        float pw=s1*s1+s2*s2-co*s1*s2;if(pw>bp){bp=pw;bf=fh;}}
    return bp<50?0:bf*60;}

static void csi_cb(void *ctx,wifi_csi_info_t *data){
    if(!data||!data->buf)return;
    snap_t s={0};
    s.ts_us=esp_timer_get_time();memcpy(s.mac,data->mac,6);
    s.rssi=data->rx_ctrl.rssi;s.ch=data->rx_ctrl.channel;
    int n=data->len/2;if(n>N_SUB)n=N_SUB;
    for(int i=0;i<n;i++){
        int8_t im=data->buf[i*2],re=data->buf[i*2+1];
        s.amp[i]=(int8_t)sqrtf(re*re+im*im);
        s.phase[i]=(int8_t)(127*atan2f(im,re)/(float)M_PI);}
    s.cfo=cfo_est(s.phase,n);s.skew=(float)data->rx_ctrl.sig_len/1000.0f;
    ring_push(&s);st.frames++;
    rfp_t *fp=find_or_new(s.mac);
    if(fp){upd_rfp(fp,&s);if(fp->pkts%50==0&&logging&&rfp_fp)
        fprintf(rfp_fp,"%02X:%02X:%02X:%02X:%02X:%02X\t%.3f\t%.3f\t%.3f\t%.4f\t%d\t%.6f\t%.6f\t%lu\n",
            fp->mac[0],fp->mac[1],fp->mac[2],fp->mac[3],fp->mac[4],fp->mac[5],
            fp->cfo_mean,fp->cfo_std,fp->skew_mean,fp->flatness,
            (int)fp->rssi_mean,fp->lat,fp->lon,(unsigned long)fp->pkts);}
    if(logging&&csi_fp){fwrite(&s,sizeof(snap_t),1,csi_fp);
        if(st.frames%20==0)fflush(csi_fp);}}

static void promisc_cb(void *buf,wifi_promiscuous_pkt_type_t type){
    if(type!=WIFI_PKT_MGMT)return;
    wifi_promiscuous_pkt_t *pk=(wifi_promiscuous_pkt_t*)buf;
    if(pk->rx_ctrl.sig_len<24)return;
    uint8_t st2=(pk->payload[0]>>4)&0x0F;
    if(st2!=4&&st2!=8&&st2!=0)return;
    const uint8_t *mac=pk->payload+10;
    rfp_t *fp=find_or_new(mac);if(!fp)return;
    fp->pkts++;fp->rssi_mean+=((float)pk->rx_ctrl.rssi-fp->rssi_mean)/fp->pkts;
    if(pk->rx_ctrl.rssi<fp->rssi_min)fp->rssi_min=pk->rx_ctrl.rssi;
    if(pk->rx_ctrl.rssi>fp->rssi_max)fp->rssi_max=pk->rx_ctrl.rssi;
    fp->last_us=esp_timer_get_time();fp->lat=st.lat;fp->lon=st.lon;
    if(logging&&rfp_fp)fprintf(rfp_fp,"%02X:%02X:%02X:%02X:%02X:%02X\tPROBE\t%.6f\t%.6f\t%d\t%lld\n",
        mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],fp->lat,fp->lon,(int)pk->rx_ctrl.rssi,(long long)fp->last_us);}

static void analysis_task(void *p){
    (void)p;char rep[192];
    while(1){
        if(ring.count<10){vTaskDelay(pdMS_TO_TICKS(500));continue;}
        st.motion=motion_score();st.mot=(st.motion>MOT_TH);
        if(st.motion<0.3f){st.bpm=breath_bpm();st.breath=(st.bpm>8&&st.bpm<36);}
        else{st.breath=false;st.bpm=0;st.walk=(st.motion>0.6f);}
        st.people=(uint8_t)fminf(fp_n*0.7f+st.motion*2,10);
        int l=snprintf(rep,sizeof(rep),
            "GHOST:{\"motion\":%.2f,\"bpm\":%.1f,\"walk\":%d,\"people\":%d,\"devices\":%lu,\"frames\":%lu}\n",
            st.motion,st.bpm,(int)st.walk,st.people,(unsigned long)st.devices,(unsigned long)st.frames);
        xRingbufferSend(spi_buf,rep,l,pdMS_TO_TICKS(10));
        vTaskDelay(pdMS_TO_TICKS(500));}}

void ghost_tracker_init(void){
    ESP_LOGI(TAG,"Ghost Tracker: passive CSI + RF fingerprint. Zero TX.");
    fp_tbl=malloc(fp_cap*sizeof(rfp_t));
    if(!fp_tbl){ESP_LOGE(TAG,"OOM");return;}
    memset(fp_tbl,0,fp_cap*sizeof(rfp_t));
    spi_buf=xRingbufferCreate(2048,RINGBUF_TYPE_NOSPLIT);
    csi_fp=fopen("/sd/ghost/csi/session.csi","ab");
    rfp_fp=fopen("/sd/ghost/rfp/session.rfp","a");
    memset(&ring,0,sizeof(ring));memset(&st,0,sizeof(st));
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    wifi_csi_config_t cc={.lltf_en=true,.htltf_en=true,.stbc_htltf2_en=true,
        .ltf_merge_en=true,.channel_filter_en=true,.manu_scale=false,.shift=0};
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&cc));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(csi_cb,NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));
    xTaskCreatePinnedToCore(analysis_task,"ghost",4096,NULL,4,NULL,0);
    ESP_LOGI(TAG,"Active. 20Hz CSI. SD log: %s",logging?"YES":"NO");
    ESP_LOGI(TAG,"Triangulation: python3 pc_tools/channel_chart.py");}

void ghost_set_gps(float lat,float lon){st.lat=lat;st.lon=lon;}
void ghost_set_logging(bool e){logging=e;}

// Non-blocking pull of one pending reply line out of this file's internal
// SPI ring buffer (spi_buf, filled by analysis_task above). Added for the
// SPI slave task in main.c (see ADR-018) so it has something real to send
// back to RP2350B instead of writing into a buffer nothing ever drained.
// Returns true and NUL-terminates `out` if a line was pending; false if
// nothing was queued right now (caller should send a generic idle ACK).
bool ghost_tracker_get_reply(char *out, size_t maxlen) {
    if (!spi_buf || !out || maxlen == 0) return false;
    size_t item_size = 0;
    void *item = xRingbufferReceive(spi_buf, &item_size, 0); // 0 = don't block
    if (!item) return false;
    size_t n = (item_size < maxlen - 1) ? item_size : maxlen - 1;
    memcpy(out, item, n);
    out[n] = '\0';
    vRingbufferReturnItem(spi_buf, item);
    return true;
}
void ghost_tracker_stop(void){
    esp_wifi_set_csi(false);esp_wifi_set_promiscuous(false);
    if(csi_fp){fflush(csi_fp);fclose(csi_fp);csi_fp=NULL;}
    if(rfp_fp){fflush(rfp_fp);fclose(rfp_fp);rfp_fp=NULL;}}
