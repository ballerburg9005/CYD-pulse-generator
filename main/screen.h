#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct {unsigned rate,width,carrier,gate,level,mode;bool running,touch_feedback;int touch_x,touch_y;} ui_state_t;
static inline unsigned period_us(unsigned rate){return (10000000u+rate/2)/rate;}
esp_err_t screen_init(void);
void screen_draw(const ui_state_t *s);
bool screen_poll_touch(int *x,int *y);
void screen_set_brightness(uint8_t value);
