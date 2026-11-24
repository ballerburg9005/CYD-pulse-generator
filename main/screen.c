/* Native graphics: no LVGL, framebuffer or PSRAM required. */
#include "screen.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_i80.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define W 320
#define H 240
#define STRIP 16
#define I2C_PORT I2C_NUM_0
#define TOUCH_ADDR 0x15

/* Kconfig defaults of 'n' are omitted from sdkconfig.h. */
#ifndef CONFIG_PULSE_LCD_ROTATE_CW
#define CONFIG_PULSE_LCD_ROTATE_CW 0
#endif
#ifndef CONFIG_PULSE_DISPLAY_FLIP_180
#define CONFIG_PULSE_DISPLAY_FLIP_180 0
#endif
#ifndef CONFIG_PULSE_LCD_MIRROR_Y
#define CONFIG_PULSE_LCD_MIRROR_Y 0
#endif
#ifndef CONFIG_PULSE_TOUCH_SWAP_XY
#define CONFIG_PULSE_TOUCH_SWAP_XY 0
#endif
#ifndef CONFIG_PULSE_TOUCH_MIRROR_X
#define CONFIG_PULSE_TOUCH_MIRROR_X 0
#endif
#ifndef CONFIG_PULSE_TOUCH_MIRROR_Y
#define CONFIG_PULSE_TOUCH_MIRROR_Y 0
#endif

typedef struct { char ch; uint8_t row[7]; } font_glyph_t;
static const font_glyph_t FONT[] = {
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,14}}, {'H',{17,17,17,31,17,17,17}},
 {'I',{31,4,4,4,4,4,31}}, {'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
 {' ',{0,0,0,0,0,0,0}}, {':',{0,4,4,0,4,4,0}},
 {'.',{0,0,0,0,0,12,12}}, {'-',{0,0,0,31,0,0,0}},
 {'/',{1,1,2,4,8,16,16}}, {'%',{25,26,2,4,8,11,19}},
 {'+',{0,4,4,31,4,4,0}}, {'!',{4,4,4,4,4,0,4}},
 {'?',{14,17,1,2,4,0,4}}, {'@',{14,17,23,21,23,16,14}},
 {'_',{0,0,0,0,0,0,31}}, {'*',{0,21,14,31,14,21,0}},
 {'=',{0,0,31,0,31,0,0}}, {',',{0,0,0,0,4,4,8}},
 {'[',{14,8,8,8,8,8,14}}, {']',{14,2,2,2,2,2,14}},
 {'a',{0,0,14,1,15,17,15}},
 {'b',{16,16,30,17,17,17,30}},
 {'c',{0,0,14,17,16,17,14}},
 {'d',{1,1,15,17,17,17,15}},
 {'e',{0,0,14,17,31,16,14}},
 {'f',{6,9,8,28,8,8,8}},
 {'g',{0,15,17,17,15,1,14}},
 {'h',{16,16,30,17,17,17,17}},
 {'i',{4,0,12,4,4,4,14}},
 {'j',{2,0,6,2,2,18,12}},
 {'k',{16,16,18,20,24,20,18}},
 {'l',{12,4,4,4,4,4,14}},
 {'m',{0,0,26,21,21,21,21}},
 {'n',{0,0,30,17,17,17,17}},
 {'o',{0,0,14,17,17,17,14}},
 {'p',{0,30,17,17,30,16,16}},
 {'q',{0,15,17,17,15,1,1}},
 {'r',{0,0,22,25,16,16,16}},
 {'s',{0,0,15,16,14,1,30}},
 {'t',{8,8,28,8,8,9,6}},
 {'u',{0,0,17,17,17,19,13}},
 {'v',{0,0,17,17,17,10,4}},
 {'w',{0,0,17,17,21,21,10}},
 {'x',{0,0,17,10,4,10,17}},
 {'y',{0,0,17,17,15,1,14}},
 {'z',{0,0,31,2,4,8,31}},
};
static uint8_t *strip;
static SemaphoreHandle_t finished;
static esp_lcd_i80_bus_handle_t bus;
static esp_lcd_panel_io_handle_t io;
static esp_lcd_panel_handle_t panel;
static int top;
static const uint16_t bg=0x0000,card=0x1925,ink=0xF79D,accent=0xC6BC,muted=0xADD8;

static const uint8_t *glyph(char c)
{
    for (unsigned i = 0; i < sizeof(FONT)/sizeof(FONT[0]); ++i)
        if (FONT[i].ch == c) return FONT[i].row;
    static const uint8_t question[7]={14,17,1,2,4,0,4};
    return question;
}
static bool transfer_done(esp_lcd_panel_io_handle_t h, esp_lcd_panel_io_event_data_t *e, void *ctx)
{
    (void)h; (void)e;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)ctx, &wake);
    return wake == pdTRUE;
}
static void rect(int x, int y, int w, int h, uint16_t c)
{
    if (w <= 0 || h <= 0) return;
    int xa = x < 0 ? 0 : x, xb = x+w > W ? W : x+w;
    int ya = y < top ? top : y, yb = y+h > top+STRIP ? top+STRIP : y+h;
    if (xa >= xb || ya >= yb) return;
    for (int yy=ya; yy<yb; ++yy) {
        uint8_t *p = strip + ((size_t)(yy-top)*W + xa)*2u;
        for (int xx=xa; xx<xb; ++xx) { *p++ = (uint8_t)(c >> 8); *p++=(uint8_t)c; }
    }
}
static void rounded(int x,int y,int w,int h,int radius,uint16_t c)
{
    if (radius <= 0) { rect(x,y,w,h,c); return; }
    for (int dy=0;dy<h;++dy) {
        int cut=0;
        if (dy<radius) {
            int n=radius-1-dy;
            while (cut<radius && (radius-cut-1)*(radius-cut-1)+n*n>=radius*radius) cut++;
        } else if (dy>=h-radius) {
            int n=dy-(h-radius);
            while (cut<radius && (radius-cut-1)*(radius-cut-1)+n*n>=radius*radius) cut++;
        }
        rect(x+cut,y+dy,w-2*cut,1,c);
    }
}
static void text(int x,int y,const char *s,int scale,uint16_t c)
{
    for (int index=0;s[index];index++) {
        const uint8_t *g=glyph(s[index]);
        int gx=x+index*6*scale;
        for (int row=0;row<7;++row)
            for (int col=0;col<5;++col)
                if (g[row] & (1u<<(4-col))) rect(gx+col*scale,y+row*scale,scale,scale,c);
    }
}
static int text_w(const char *s,int scale) { return (int)strlen(s)*6*scale; }
static void center(int y,const char *s,int scale,uint16_t c)
{ text((W-text_w(s,scale))/2,y,s,scale,c); }
static void button(int x,int y,int w,int h,const char *label,bool selected)
{
    rounded(x,y,w,h,8,selected?accent:card);
    int scale = text_w(label,2) <= w-6 ? 2:1;
    text(x+(w-text_w(label,scale))/2,y+(h-7*scale)/2,label,scale,selected?bg:ink);
}

static const char *const modes[]={"LOGIC PULSE","SPEAKER SINE","SPEAKER TRIANGLE","SPEAKER SQUARE","SINE BURST","SQUARE BURST","NOISE BURST"};
void screen_draw(const ui_state_t *s){
    char b[48];
    for(top=0;top<H;top+=STRIP){
        rect(0,top,W,STRIP,bg);
        center(6,"BENCH SIGNAL GENERATOR",2,ink);
        snprintf(b,sizeof b,"%u.%u HZ / %u US PERIOD",s->rate/10,s->rate%10,period_us(s->rate));
        center(25,b,1,accent);
        button(20,38,280,30,modes[s->mode],false);
        if(s->running){
            center(87,"OUTPUT ACTIVE",3,accent);
            center(130,"TAP ANYWHERE TO STOP",2,ink);
            button(22,180,276,46,"STOP",true);
        }else{
            const char *f[]={"-5","-.1","+.1","+5"};
            for(int i=0;i<4;i++)button(20+i*71,75,67,30,f[i],false);
            if(s->mode==0)snprintf(b,sizeof b,"HIGH TIME %u US",s->width);
            else if(s->mode>=4)snprintf(b,sizeof b,"TONE %u HZ / GATE %u%%",s->carrier,s->gate);
            else snprintf(b,sizeof b,"DAC LEVEL %u%%",s->level);
            center(112,b,1,muted);
            if(!s->mode){
                const char *w[]={"-100","-10","+10","+100"};
                for(int i=0;i<4;i++)button(20+i*71,126,67,32,w[i],false);
            }else if(s->mode>=4){
                button(20,126,67,32,"TONE -",false);button(91,126,67,32,"TONE +",false);
                button(162,126,67,32,"GATE -",false);button(233,126,67,32,"GATE +",false);
            }else{
                button(20,126,138,32,"LEVEL -",false);button(162,126,138,32,"LEVEL +",false);
            }
            button(22,174,276,43,"START / 15 SEC",true);
            center(226,"BENCH ONLY / NO BODY CONNECTION",1,muted);
        }
        if(s->touch_feedback)rounded(s->touch_x-4,s->touch_y-4,9,9,4,ink);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel,0,top,W,top+STRIP,strip));
        if(xSemaphoreTake(finished,pdMS_TO_TICKS(700))!=pdTRUE)abort();
    }
}
void screen_set_brightness(uint8_t brightness)
{
    if(brightness<3) brightness=3;
    if(brightness>100) brightness=100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,brightness*1023/100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0);
}
esp_err_t screen_init(void)
{
    gpio_config_t rd={.pin_bit_mask=1ULL<<2,.mode=GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&rd));
    gpio_set_level(GPIO_NUM_2,1);
    const ledc_timer_config_t timer={
        .speed_mode=LEDC_LOW_SPEED_MODE,.timer_num=LEDC_TIMER_0,
        .duty_resolution=LEDC_TIMER_10_BIT,.freq_hz=4000,.clk_cfg=LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    const ledc_channel_config_t channel={
        .gpio_num=0,.speed_mode=LEDC_LOW_SPEED_MODE,
        .channel=LEDC_CHANNEL_0,.intr_type=LEDC_INTR_DISABLE,
        .timer_sel=LEDC_TIMER_0,.duty=0,.hpoint=0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
    finished=xSemaphoreCreateBinary();
    if(!finished)return ESP_ERR_NO_MEM;
    const esp_lcd_i80_bus_config_t bc={
        .clk_src=LCD_CLK_SRC_DEFAULT,.dc_gpio_num=GPIO_NUM_16,
        .wr_gpio_num=GPIO_NUM_4,
        .data_gpio_nums={15,13,12,14,27,25,33,32},.bus_width=8,
        .max_transfer_bytes=W*STRIP*2
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bc,&bus));
    const esp_lcd_panel_io_i80_config_t ic={
        .cs_gpio_num=GPIO_NUM_17,.pclk_hz=10000000,
        .trans_queue_depth=2,.lcd_cmd_bits=8,.lcd_param_bits=8,
        .dc_levels={.dc_idle_level=0,.dc_cmd_level=0,.dc_dummy_level=0,.dc_data_level=1}
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(bus,&ic,&io));
    esp_lcd_panel_io_callbacks_t callbacks={.on_color_trans_done=transfer_done};
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io,&callbacks,finished));
    const esp_lcd_panel_dev_config_t dc={
        .reset_gpio_num=-1,.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_BGR,.bits_per_pixel=16
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io,&dc,&panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel,true));
    /* Button/TF-up installation: rotate the already landscape LCD 180°.
     * This inverts BOTH mirror flags. Touch axes are flipped below too. */
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel,
                CONFIG_PULSE_LCD_ROTATE_CW ^ CONFIG_PULSE_DISPLAY_FLIP_180,
                CONFIG_PULSE_LCD_MIRROR_Y ^ CONFIG_PULSE_DISPLAY_FLIP_180));
    /* Inversion made the BLACK theme physically white on this panel. */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel,false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));
    strip=heap_caps_malloc(W*STRIP*2,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
    if(!strip)return ESP_ERR_NO_MEM;
    screen_set_brightness(30);
    ESP_LOGI("pulse_screen","320x240 landscape ST7789 i80 initialized");
    return ESP_OK;
}
bool screen_poll_touch(int *x,int *y){
    uint8_t reg=0x02,d[5];
    esp_err_t err=i2c_master_write_read_device(I2C_PORT,TOUCH_ADDR,
                       &reg,1,d,5,pdMS_TO_TICKS(22));
    if(err!=ESP_OK || (d[0]&0x0F)==0)return false;
    int raw_x=((d[1]&0x0f)<<8)|d[2];
    int raw_y=((d[3]&0x0f)<<8)|d[4];
    /* The board's CST816S axes run opposite the displayed landscape
       orientation after swapping ST7789 XY. Correct both axes here,
       independently of the LCD mirror configuration. */
    int xx=CONFIG_PULSE_LCD_ROTATE_CW?raw_y:319-raw_y;
    int yy=CONFIG_PULSE_LCD_ROTATE_CW?239-raw_x:raw_x;
    if(CONFIG_PULSE_TOUCH_SWAP_XY){int tmp=xx;xx=yy;yy=tmp;}
    if(CONFIG_PULSE_TOUCH_MIRROR_X)xx=W-1-xx;
    if(CONFIG_PULSE_TOUCH_MIRROR_Y)yy=H-1-yy;
    /* Match the physical 180-degree LCD rotation before edge calibration. */
    if(CONFIG_PULSE_DISPLAY_FLIP_180){
        xx=W-1-xx;
        yy=H-1-yy;
    }
    /* Compensate for capacitive active area falling short of LCD edges.
       Exaggerate offsets from center and saturate at screen boundaries;
       this retains central precision and keeps existing orientation. */
    xx=W/2+(xx-W/2)*CONFIG_PULSE_TOUCH_EDGE_GAIN/100;
    yy=H/2+(yy-H/2)*CONFIG_PULSE_TOUCH_EDGE_GAIN/100;
    if(xx<0)xx=0;
    if(xx>=W)xx=W-1;
    if(yy<0)yy=0;
    if(yy>=H)yy=H-1;
    *x=xx; *y=yy; return true;
}
