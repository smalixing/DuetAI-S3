#ifndef _BSP_GPIO_H
#define _BSP_GPIO_H
#include "driver/gpio.h"
esp_err_t bsp_gpio_init(int32_t gpio_num,gpio_mode_t mode,bool active_level,gpio_int_type_t intr_type);
void bsp_gpio_input_register_cb(uint8_t id,void (*cb)(uint8_t id));
void bsp_gpio_base_set(int32_t gpio_num,bool value);
void bsp_gpio_set(uint8_t id,bool value);
int bsp_gpio_get(uint8_t id);
void bsp_gpio_input_init(void);
void bsp_gpio_isr_add(uint8_t id);
void bsp_gpio_isr_remove(uint8_t id);
#endif // !_BSP_RTM_H