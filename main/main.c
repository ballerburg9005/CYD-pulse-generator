#include "screen.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/dac_oneshot.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <math.h>
#include <stdlib.h>
#define OUT GPIO_NUM_23
static ui_state_t ui={.rate=50,.width=1000,.carrier=1000,.gate=30,.level=50};
static ui_state_t cfg;
static rmt_channel_handle_t ch;
static rmt_encoder_handle_t enc;
static rmt_symbol_word_t words[32];
static dac_oneshot_handle_t dac;
static esp_timer_handle_t timer;
static SemaphoreHandle_t lock;
static bool logic_on,audio_on;
static int64_t t0;
static int8_t sine[256];
static uint32_t rng=0x1337;
static void sample(void *arg){
    (void)arg;
    if(xSemaphoreTake(lock,0)!=pdTRUE)return;
    if(audio_on){
        uint64_t elapsed=(uint64_t)(esp_timer_get_time()-t0);
        unsigned p=period_us(cfg.rate);
        unsigned phase=(unsigned)((elapsed%p)*256u/p);
        int v=0;
        if(cfg.mode==1)v=sine[phase];
        else if(cfg.mode==2)v=phase<128?2*(int)phase-127:383-2*(int)phase;
        else if(cfg.mode==3)v=phase<128?127:-127;
        else if(elapsed%p<(uint64_t)p*cfg.gate/100u){
            unsigned tone=(unsigned)((elapsed*cfg.carrier*256u/1000000u)&255u);
            if(cfg.mode==4)v=sine[tone];
            else if(cfg.mode==5)v=tone<128?127:-127;
            else{rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;v=(int)(rng&255u)-128;}
        }
        int span=cfg.mode>=4?16:(int)cfg.level*32/100;
        dac_oneshot_output_voltage(dac,(uint8_t)(128+v*span/128));
    }
    xSemaphoreGive(lock);
}
static void output_init(void){
    ESP_ERROR_CHECK(gpio_reset_pin(OUT));
    ESP_ERROR_CHECK(gpio_set_level(OUT,0));
    ESP_ERROR_CHECK(gpio_set_direction(OUT,GPIO_MODE_OUTPUT));
    rmt_tx_channel_config_t c={.gpio_num=OUT,.clk_src=RMT_CLK_SRC_DEFAULT,
        .resolution_hz=1000000,.mem_block_symbols=64,.trans_queue_depth=1};
    ESP_ERROR_CHECK(rmt_new_tx_channel(&c,&ch));
    rmt_copy_encoder_config_t ec={0};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&ec,&enc));
    dac_oneshot_config_t dc={.chan_id=DAC_CHAN_1}; /* GPIO26 */
    ESP_ERROR_CHECK(dac_oneshot_new_channel(&dc,&dac));
    ESP_ERROR_CHECK(dac_oneshot_output_voltage(dac,128));
    lock=xSemaphoreCreateMutex();if(!lock)abort();
    for(int i=0;i<256;i++)sine[i]=(int8_t)lround(127*sin(6.283185307179586*i/256));
    esp_timer_create_args_t args={.callback=sample,.dispatch_method=ESP_TIMER_TASK,
        .name="audio_test",.skip_unhandled_events=true};
    ESP_ERROR_CHECK(esp_timer_create(&args,&timer));
}
static void stop(void){
    if(logic_on){ESP_ERROR_CHECK(rmt_disable(ch));logic_on=false;}
    xSemaphoreTake(lock,portMAX_DELAY);
    if(audio_on){
        audio_on=false;
        ESP_ERROR_CHECK(esp_timer_stop(timer));
        ESP_ERROR_CHECK(dac_oneshot_output_voltage(dac,128));
    }
    xSemaphoreGive(lock);ui.running=false;
}
static void start(void){
    if(ui.mode){
        xSemaphoreTake(lock,portMAX_DELAY);
        cfg=ui;t0=esp_timer_get_time();audio_on=true;
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer,100));
        xSemaphoreGive(lock);
    }else{
        unsigned n=0,low=period_us(ui.rate)-ui.width;
        uint16_t ticks[64];ticks[n++]=ui.width;
        while(low){unsigned t=low>32767?32767:low;ticks[n++]=t;low-=t;}
        if(n&1){
            if(ticks[n-1]>1){ticks[n-1]--;ticks[n++]=1;}
            else{ticks[n-2]--;ticks[n++]=1;}
        }
        for(unsigned i=0;i<n/2;i++)words[i]=(rmt_symbol_word_t){
            .duration0=ticks[2*i],.level0=i==0,
            .duration1=ticks[2*i+1],.level1=0};
        ESP_ERROR_CHECK(rmt_enable(ch));logic_on=true;
        rmt_transmit_config_t tc={.loop_count=-1,.flags.eot_level=0};
        esp_err_t e=rmt_transmit(ch,enc,words,n/2*sizeof(words[0]),&tc);
        if(e!=ESP_OK){stop();ESP_LOGE("pulse","RMT start failed: %s",esp_err_to_name(e));return;}
    }
    ui.running=true;
}
static bool in(int x,int y,int l,int t,int w,int h){return x>=l&&x<l+w&&y>=t&&y<t+h;}
static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static void touch(int x,int y){
    if(ui.running){stop();return;}
    if(in(x,y,20,38,280,30)){ui.mode=(ui.mode+1)%7;return;}
    static const int f[]={-50,-1,1,50},w[]={-100,-10,10,100};
    for(int i=0;i<4;i++){
        if(in(x,y,20+i*71,75,67,30)){ui.rate=clamp((int)ui.rate+f[i],5,400);return;}
        if(ui.mode>0&&ui.mode<4)continue;
        if(in(x,y,20+i*71,126,67,32)){
            if(!ui.mode)ui.width=clamp((int)ui.width+w[i],10,10000);
            else if(i<2)ui.carrier=clamp((int)ui.carrier+(i?100:-100),200,2000);
            else ui.gate=clamp((int)ui.gate+(i==3?10:-10),10,90);
            return;
        }
    }
    if(ui.mode>0&&ui.mode<4){
        if(in(x,y,20,126,138,32)){ui.level=clamp((int)ui.level-10,0,100);return;}
        if(in(x,y,162,126,138,32)){ui.level=clamp((int)ui.level+10,0,100);return;}
    }
    if(in(x,y,22,174,276,43))start();
}
void app_main(void){
    output_init();
    const i2c_config_t c={.mode=I2C_MODE_MASTER,.sda_io_num=21,.scl_io_num=22,
        .sda_pullup_en=GPIO_PULLUP_ENABLE,.scl_pullup_en=GPIO_PULLUP_ENABLE,.master.clk_speed=100000};
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0,&c));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0,I2C_MODE_MASTER,0,0,0));
    ESP_ERROR_CHECK(screen_init());screen_set_brightness(65);screen_draw(&ui);
    int64_t last_seen=0,marker_until=0;bool armed=true;
    for(;;){
        bool dirty=false;int64_t now=esp_timer_get_time();
        int x,y;
        if(screen_poll_touch(&x,&y)){
            last_seen=esp_timer_get_time();
            if(armed){armed=false;touch(x,y);ui.touch_feedback=true;ui.touch_x=x;ui.touch_y=y;marker_until=last_seen+350000;dirty=true;}
        }else if(esp_timer_get_time()-last_seen>=180000)armed=true;
        if(ui.touch_feedback&&now>=marker_until){ui.touch_feedback=false;dirty=true;}
        if(dirty)screen_draw(&ui);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
