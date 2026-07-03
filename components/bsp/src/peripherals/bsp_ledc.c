#include "string.h"
#include "bsp_ledc.h"
#include "bsp_board.h"
#include "esp_compiler.h"
#include "esp_log.h"
static char *TAG = "LCDC";
void bsp_lcdc_init(int gpio_num,ledc_timer_t timer_num,ledc_timer_t timer_sel,ledc_channel_t channel)
{
    
    if (gpio_num == GPIO_NUM_NC) {
        ESP_LOGI(TAG, "No lcd backlight io specified");
        return;
    }
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = timer_num,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .freq_hz          = 1000,  // Set output frequency at 1 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = channel,
        .timer_sel      = timer_sel,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = gpio_num,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    
}

void bsp_lcdc_set_level(int level,ledc_channel_t channel)
{
    const int ledc_duty_all = 8191;
    if (level < 0) {
        level = 0;
    } else if (level > 100) {
        level = 100;
    }
    uint32_t duty = ledc_duty_all * level / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}