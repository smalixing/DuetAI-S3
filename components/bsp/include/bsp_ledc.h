#ifndef _BSP_LCDC_H_
#define _BSP_LCDC_H_
#include "driver/ledc.h"
void bsp_lcdc_init(int gpio_num,ledc_timer_t timer_num,ledc_timer_t timer_sel,ledc_channel_t channel);
void bsp_lcdc_set_level(int level,ledc_channel_t channel);
#endif