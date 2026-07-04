/*
 * @Descripttion: 
 * @version: 
 * @Author: oliver
 * @Date: 2022-11-04 09:42:00
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt.h"
#include "driver/gpio.h"
#include "bsp_rtm.h"
#include "bsp_board.h"
static const char *TAG = "rtm";

int bsp_rmt_tx_init(uint8_t gpio_num,uint8_t rmt_channel)
{
    rmt_item32_t *items = NULL;
    rmt_config_t rmt_tx_config = RMT_DEFAULT_CONFIG_TX(gpio_num, rmt_channel);
    rmt_tx_config.tx_config.carrier_en = true;
    rmt_config(&rmt_tx_config);
    rmt_driver_install(rmt_channel, 0, 0);
    return 0;
}

void bsp_rmt_default_init()
{
    const board_res_desc_t *brd = bsp_board_get_description();
    for(uint8_t i = 0;i<brd->rtm_len;i++)
    {
        bsp_rmt_tx_init(brd->rtm_param_tb[i].rtm_gpio_num,brd->rtm_param_tb->channel);
    }
}

void bsp_rmt_ir_set_freq(uint8_t id, int freq)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if (id >= brd->rtm_len)
        return;
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(brd->rtm_param_tb[id].rtm_gpio_num, brd->rtm_param_tb[id].channel);
    // enable the carrier to be able to hear the Morse sound
    // if the RMT_TX_GPIO is connected to a speaker
    config.mem_block_num = 4;
    config.tx_config.carrier_en = true;
    config.tx_config.carrier_duty_percent = 50;
    config.tx_config.carrier_freq_hz = freq;
    ESP_ERROR_CHECK(rmt_driver_uninstall(config.channel));
    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // for debug
    uint32_t counter_clk_hz = 0;
    rmt_get_counter_clock(config.channel, &counter_clk_hz);
    float ratio = (float)counter_clk_hz / 1e6;
    ESP_LOGI(TAG, "%s: clk=%d, ratio = %f\n", __func__, counter_clk_hz, ratio);
}

void bsp_rmt_ir_code_send(uint8_t id, uint16_t *buf, int len)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if (id >= brd->rtm_len)
        return;

    int rmt_cnt = (len + 1) / 2 + 1;
    rmt_item32_t *rmt_buf = malloc(rmt_cnt * sizeof(rmt_item32_t));
    for (int i = 0; i < rmt_cnt - 1; i ++) {
        rmt_buf[i].level0 = 1;
        rmt_buf[i].duration0 = buf[2 * i];
        rmt_buf[i].level1 = 0;
        if (2 * i + 1 < len) {
            rmt_buf[i].duration1 = buf[2 * i + 1];
        } else {
            rmt_buf[i].duration1 = 0;
        }
    }
    rmt_buf[rmt_cnt - 1].val = 0;

    rmt_write_items(brd->rtm_param_tb[id].channel, rmt_buf, rmt_cnt, true);

    ESP_LOGI(TAG, "%s: len = %d, rmt_cnt = %d\n", __func__, len, rmt_cnt);
    free(rmt_buf);
}
