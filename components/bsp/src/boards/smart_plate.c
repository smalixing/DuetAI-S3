/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "bsp_board.h"
#include "driver/uart.h"
#include "driver/rmt.h"
#include "button.h"
#include "bsp_btn.h"
#include "driver/i2c.h"
#include "bsp_adc.h"
#include "bsp_uart.h"
#include "bsp_rtm.h"
#include "bsp_gpio.h"
#include "bsp_i2c.h"
#include "bsp_ledc.h"
#include "bsp_lcd.h"
#include "smart_plate.h"

static const board_button_t g_btns[] = {
    {0, 0,  GPIO_NUM_10,  0},
    {1, 0,  GPIO_NUM_11, 0},
    {2, 0, GPIO_NUM_12, 0},
    {3, 0,  GPIO_NUM_13, 0},
};

//static const board_adc_t g_adcs[] = {
//    {ADC_UNIT_2,ADC2_CHANNEL_9}
//};

static const board_uart_param_t g_bsp_uart_gpio_tb[] = {
    {
        .uart_port = UART_NUM_2,
        .tx_gpio_num = GPIO_NUM_48,
        .rx_gpio_num = GPIO_NUM_47,
        .en_gpio_num = GPIO_NUM_42,
        .uart_mode = UART_MODE_RS485_HALF_DUPLEX,
        .rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bit = UART_STOP_BITS_1
    },
    {
        .uart_port = UART_NUM_1,
        .tx_gpio_num = GPIO_NUM_9,
        .rx_gpio_num = GPIO_NUM_45,
        .en_gpio_num = GPIO_NUM_NC,
        .uart_mode = UART_MODE_UART,
        .rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bit = UART_STOP_BITS_1
    },
};

static const board_rtm_param_t g_bsp_rtm_param_tb[] = {
    {GPIO_NUM_46,RMT_CHANNEL_0},
};

static const char bsp_output_gpio_tb[] = {GPIO_NUM_41,GPIO_NUM_40,GPIO_NUM_39};

static const board_input_gpio_param_t bsp_input_gpio_tb[] = {
    {GPIO_NUM_14,GPIO_INTR_ANYEDGE}
};

static const board_ledc_param_t bsp_board_ledc_tb[] = {
    {GPIO_NUM_17,LEDC_TIMER_1,LEDC_TIMER_1,LEDC_CHANNEL_1},
    {GPIO_NUM_18,LEDC_TIMER_1,LEDC_TIMER_1,LEDC_CHANNEL_2},
    {GPIO_NUM_19,LEDC_TIMER_1,LEDC_TIMER_1,LEDC_CHANNEL_3},
    {GPIO_NUM_38,LEDC_TIMER_1,LEDC_TIMER_1,LEDC_CHANNEL_4},
};


static const board_res_desc_t g_board_smart_plate_res = {
    .FUNC_LCD_EN =     (1),
    .LCD_BUS_WIDTH =   (1),
    .LCD_IFACE_SPI =   (1),
    /*!< 0: NT35510, 1: ST7789, 2: GC9A01, 3: GC9307 (see bsp_lcd_init) */
    .LCD_DISP_IC_ST =  (3),
    .LCD_WIDTH =       (240),
    .LCD_HEIGHT =      (320),
    .LCD_FREQ =        (40 * 1000 * 1000),
    #ifdef LCD_PANEL_USER_3_WIRE_MODE
    .LCD_CMD_BITS =    16,
    #else
    .LCD_CMD_BITS =    8,
    #endif
    .LCD_PARAM_BITS =  8,
    .LCD_HOST =        (SPI2_HOST),

    .LCD_SWAP_XY =     (false),
    .LCD_MIRROR_X =    (true),
    .LCD_MIRROR_Y =    (false),
    #ifdef LCD_PANEL_USER_3_WIRE_MODE
    .LCD_COLOR_INV =   (true),
    #else
    .LCD_COLOR_INV =   (false),
    #endif
    .LCD_COLOR_SPACE = ESP_LCD_COLOR_SPACE_BGR,

    .lcd_bl_use_pwm   = (true),
    .LCD_BL_PWM_CH    = LEDC_CHANNEL_0,
    .LCD_BL_PWM_TIMER = LEDC_TIMER_0,

    .GPIO_LCD_BL =     (GPIO_NUM_15),
    .GPIO_LCD_BL_ON =  (1),
    .GPIO_LCD_CS =     (GPIO_NUM_2),
    .GPIO_LCD_RST =    (GPIO_NUM_5),
    #ifdef LCD_PANEL_USER_3_WIRE_MODE
    .GPIO_LCD_DC =     (GPIO_NUM_NC),
    #else
    .GPIO_LCD_DC =     (GPIO_NUM_1),
    #endif
    .GPIO_LCD_CLK =    (GPIO_NUM_3),
    .GPIO_LCD_DIN =    (GPIO_NUM_4),
    .GPIO_LCD_DOUT =   (GPIO_NUM_NC),

    .BSP_INDEV_IS_TP =    (1),
    .TOUCH_PANEL_SWAP_XY =     (0),
    .TOUCH_PANEL_INVERSE_X =   (0),
    .TOUCH_PANEL_INVERSE_Y =   (0),
    .TOUCH_PANEL_I2C_ADDR = 0,
    .TOUCH_WITH_HOME_BUTTON = 0,
    .TOUCH_REST_GPIO_NUM = GPIO_NUM_6,
    .TOUCH_IRQ_GPIO_NUM = GPIO_NUM_9,

    .BSP_BUTTON_EN =   (1),
    .BUTTON_ADC_CHAN =  ADC1_CHANNEL_MAX,
    .BUTTON_TAB =  g_btns,
    .BUTTON_TAB_LEN = sizeof(g_btns) / sizeof(g_btns[0]),

    .FUNC_I2C_EN =     (1),
    .GPIO_I2C_SCL =    (GPIO_NUM_7),
    .GPIO_I2C_SDA =    (GPIO_NUM_8),
    .FUNC_I2C1_EN =    (1),
    .GPIO_I2C1_SCL =    (GPIO_NUM_45),
    .GPIO_I2C1_SDA =    (GPIO_NUM_20),
    .FUNC_SDMMC_EN =   (0),
    .SDMMC_BUS_WIDTH = (4),
    .GPIO_SDMMC_CLK =  (GPIO_NUM_NC),
    .GPIO_SDMMC_CMD =  (GPIO_NUM_NC),
    .GPIO_SDMMC_D0 =   (GPIO_NUM_NC),
    .GPIO_SDMMC_D1 =   (GPIO_NUM_NC),
    .GPIO_SDMMC_D2 =   (GPIO_NUM_NC),
    .GPIO_SDMMC_D3 =   (GPIO_NUM_NC),
    .GPIO_SDMMC_DET =  (GPIO_NUM_NC),

    .FUNC_SDSPI_EN =       (0),
    .SDSPI_HOST =          (SPI2_HOST),
    .GPIO_SDSPI_CS =       (GPIO_NUM_NC),
    .GPIO_SDSPI_SCLK =     (GPIO_NUM_NC),
    .GPIO_SDSPI_MISO =     (GPIO_NUM_NC),
    .GPIO_SDSPI_MOSI =     (GPIO_NUM_NC),

    .FUNC_SPI_EN =         (0),
    .GPIO_SPI_CS =         (GPIO_NUM_NC),
    .GPIO_SPI_MISO =       (GPIO_NUM_NC),
    .GPIO_SPI_MOSI =       (GPIO_NUM_NC),
    .GPIO_SPI_SCLK =       (GPIO_NUM_NC),

    .FUNC_RMT_EN =         (0),
    .GPIO_RMT_IR =         (GPIO_NUM_NC),
    .GPIO_RMT_LED =        (GPIO_NUM_NC),

    .FUNC_I2S_EN =         (0),
    .GPIO_I2S_LRCK =       (GPIO_NUM_NC),
    .GPIO_I2S_MCLK =       (GPIO_NUM_NC),
    .GPIO_I2S_SCLK =       (GPIO_NUM_NC),
    .GPIO_I2S_SDIN =       (GPIO_NUM_NC),
    .GPIO_I2S_DOUT =       (GPIO_NUM_NC),
    .CODEC_I2C_ADDR = 0,
    .AUDIO_ADC_I2C_ADDR = 0,

    .IMU_I2C_ADDR = 0,

    .FUNC_PWR_CTRL =       (1),
    .GPIO_PWR_CTRL =       (GPIO_NUM_16),
    .GPIO_PWR_ON_LEVEL =   (1),

    .GPIO_MUTE_NUM =   GPIO_NUM_NC,
    .GPIO_MUTE_LEVEL = 1,

    .PMOD1 = NULL,
    .PMOD2 = NULL,
    .FUNC_ADC_EN = 0,
    .adc_tb = NULL,
    .adc_len = 0,
    .FUNC_UART_EN = 1,
    .uart_param_tb = g_bsp_uart_gpio_tb,
    .uart_len = sizeof(g_bsp_uart_gpio_tb)/sizeof(board_uart_param_t),
    .FUNC_RTM_EN = 1,
    .rtm_param_tb = g_bsp_rtm_param_tb,
    .rtm_len = sizeof(g_bsp_rtm_param_tb)/sizeof(board_rtm_param_t),
    .FUNC_GPIO_EN = 1,
    .gpio_tb = bsp_output_gpio_tb,
    .gpio_tb_len = sizeof(bsp_output_gpio_tb) / sizeof(bsp_output_gpio_tb[0]),
    .gpio_input_tb = bsp_input_gpio_tb,
    .gpio_input_tb_len = sizeof(bsp_input_gpio_tb) / sizeof(bsp_input_gpio_tb[0]),
    .ledc_tb = bsp_board_ledc_tb,
    .ledc_tb_len = sizeof(bsp_board_ledc_tb) / sizeof(bsp_board_ledc_tb[0]),
};

static const char *TAG = "board";

esp_err_t bsp_board_smart_plate_init(void)
{
    ESP_LOGI("smart_plate","%s",__FUNCTION__);
    for(uint8_t i = 0;i<sizeof(bsp_board_ledc_tb)/sizeof(bsp_board_ledc_tb[0]);i++)
    {
        bsp_lcdc_init(bsp_board_ledc_tb[i].gpio_num,bsp_board_ledc_tb[i].timer_num,
        bsp_board_ledc_tb[i].timer_sel,bsp_board_ledc_tb[i].channel);
    }

    for(uint8_t i = 0;i<sizeof(bsp_output_gpio_tb)/sizeof(bsp_output_gpio_tb[0]);i++)
    {
        bsp_gpio_init(bsp_output_gpio_tb[i],GPIO_MODE_INPUT_OUTPUT,0,GPIO_INTR_DISABLE);
        bsp_gpio_base_set(bsp_output_gpio_tb[i],0);
    }
    bsp_gpio_init(g_board_smart_plate_res.GPIO_LCD_BL,GPIO_MODE_OUTPUT,0,GPIO_INTR_DISABLE);
    bsp_gpio_init(g_board_smart_plate_res.GPIO_PWR_CTRL,GPIO_MODE_OUTPUT,0,GPIO_INTR_DISABLE);
    /*!< Power up the peripheral rail (GPIO16) that feeds the LCD before
     *   bsp_lcd_init() brings up the panel and its backlight PWM. */
    bsp_gpio_base_set(g_board_smart_plate_res.GPIO_PWR_CTRL, g_board_smart_plate_res.GPIO_PWR_ON_LEVEL);
    /*!< GT911 address-select reset sequence: IRQ(GPIO9) low, pulse RST(GPIO6),
     *   then release IRQ with pull-up. */
    bsp_gpio_init(GPIO_NUM_9,GPIO_MODE_OUTPUT,0,GPIO_INTR_DISABLE);
    bsp_gpio_init(GPIO_NUM_6,GPIO_MODE_OUTPUT,0,GPIO_INTR_DISABLE);
    usleep(200);
    bsp_gpio_base_set(GPIO_NUM_6,1);
    usleep(5000);
    bsp_gpio_init(GPIO_NUM_9,GPIO_MODE_INPUT,1,GPIO_INTR_DISABLE);
    bsp_btn_init_default();
    bsp_uart_default_init();
    bsp_rmt_default_init();
    bsp_gpio_input_init();

    if(g_board_smart_plate_res.FUNC_I2C1_EN)
        bsp_i2c_init(I2C_NUM_1, 400 * 1000, g_board_smart_plate_res.GPIO_I2C1_SCL,g_board_smart_plate_res.GPIO_I2C1_SDA);
    ESP_ERROR_CHECK(bsp_lcd_init());
    return ESP_OK;
}

esp_err_t bsp_board_smart_plate_power_ctrl(power_module_t module, bool on)
{
    /* Config power control IO */
    static esp_err_t bsp_io_config_state = ESP_FAIL;
    if (ESP_OK != bsp_io_config_state) {
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << g_board_smart_plate_res.GPIO_PWR_CTRL;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        bsp_io_config_state = gpio_config(&io_conf);
    }

    /* Checko IO config result */
    if (ESP_OK != bsp_io_config_state) {
        ESP_LOGE(TAG, "Failed initialize power control IO");
        return bsp_io_config_state;
    }

    /* Control independent power domain */
    switch (module) {
    case POWER_MODULE_LCD:
        gpio_set_level(g_board_smart_plate_res.GPIO_LCD_BL, on ? (g_board_smart_plate_res.GPIO_LCD_BL_ON) : (!g_board_smart_plate_res.GPIO_LCD_BL_ON));
        break;
    case POWER_MODULE_AUDIO:
    case POWER_MODULE_ALL:
        gpio_set_level(g_board_smart_plate_res.GPIO_PWR_CTRL, on ? (g_board_smart_plate_res.GPIO_PWR_ON_LEVEL) : (!g_board_smart_plate_res.GPIO_PWR_ON_LEVEL));
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

const board_res_desc_t *bsp_board_smart_plate_get_res_desc(void)
{
    return &g_board_smart_plate_res;
}
