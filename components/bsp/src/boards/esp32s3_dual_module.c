/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "driver/uart.h"
#include "bsp_board.h"
#include "bsp_i2s.h"
#include "bsp_codec.h"
#include "bsp_lcd.h"
#include "bsp_adc.h"
#include "bsp_uart.h"
#include "button.h"
#include "bsp_btn.h"
#include "esp32s3_dual_module.h"

/*!< Audio power amplifier (PA_CTRL) enable, active high. Not part of the shared
 *   board descriptor, so it is kept local to this board. */
#define GPIO_PA_CTRL        GPIO_NUM_48
#define GPIO_PA_CTRL_ON     (1)

/*!< LCD tearing-effect (TE) input. No dedicated field in board_res_desc_t. */
#define GPIO_LCD_TE         GPIO_NUM_39

static const board_adc_t g_adc[] = {
    /*!< BAT_MEAS_ADC -> GPIO6 == ADC1_CHANNEL_5 on ESP32-S3 */
    {ADC_UNIT_1, ADC1_CHANNEL_5},
};

static const board_uart_param_t g_uart[] = {
    {
        .uart_port    = UART_NUM_1,
        .tx_gpio_num  = GPIO_NUM_3,
        .rx_gpio_num  = GPIO_NUM_21,
        .en_gpio_num  = -1,     /*!< UART_PIN_NO_CHANGE: no RTS/enable pin */
        .uart_mode    = BSP_UART_MODE_UART,
        .rate         = 115200,
        .data_bits    = BSP_UART_DATA_8_BITS,
        .parity       = BSP_UART_PARITY_DISABLE,
        .stop_bit     = BSP_UART_STOP_BITS_1,
    },
};

static const board_res_desc_t g_board_s3_dual_module_res = {
    .FUNC_LCD_EN =     (1),
    .LCD_BUS_WIDTH =   (1),
    .LCD_IFACE_SPI =   (1),
    .LCD_DISP_IC_ST =  (1),
    .LCD_WIDTH =       (240),
    .LCD_HEIGHT =      (240),
    .LCD_FREQ =        (40 * 1000 * 1000),
    .LCD_CMD_BITS =    8,
    .LCD_PARAM_BITS =  8,
    .LCD_HOST =        (SPI2_HOST),

    .LCD_SWAP_XY =     (0),
    .LCD_MIRROR_X =    (true),
    .LCD_MIRROR_Y =    (true),
    .LCD_COLOR_INV =   (false),
    .LCD_COLOR_SPACE = ESP_LCD_COLOR_SPACE_BGR,

    /*!< No dedicated backlight pin on this board; panel power comes from PERI_PWR_ON */
    .GPIO_LCD_BL =     (GPIO_NUM_NC),
    .GPIO_LCD_BL_ON =  (1),
    .GPIO_LCD_CS =     (GPIO_NUM_38),
    .GPIO_LCD_RST =    (GPIO_NUM_42),
    .GPIO_LCD_DC =     (GPIO_NUM_2),
    .GPIO_LCD_CLK =    (GPIO_NUM_1),
    .GPIO_LCD_DIN =    (GPIO_NUM_0),
    .GPIO_LCD_DOUT =   (GPIO_NUM_NC),

    .BSP_INDEV_IS_TP =    (0),
    .TOUCH_PANEL_SWAP_XY =     (0),
    .TOUCH_PANEL_INVERSE_X =   (0),
    .TOUCH_PANEL_INVERSE_Y =   (0),
    .TOUCH_PANEL_I2C_ADDR = 0,
    .TOUCH_WITH_HOME_BUTTON = 0,

    /*!< GPIO0 is used for LCD_SDA on this board, so no GPIO/ADC button is wired */
    .BSP_BUTTON_EN =   (0),
    .BUTTON_ADC_CHAN =  ADC1_CHANNEL_0,
    .BUTTON_TAB =  NULL,
    .BUTTON_TAB_LEN = 0,

    .FUNC_I2C_EN =     (1),
    .GPIO_I2C_SCL =    (GPIO_NUM_18),
    .GPIO_I2C_SDA =    (GPIO_NUM_17),

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

    .FUNC_I2S_EN =         (1),
    .GPIO_I2S_LRCK =       (GPIO_NUM_45),
    .GPIO_I2S_MCLK =       (GPIO_NUM_16),
    .GPIO_I2S_SCLK =       (GPIO_NUM_9),
    .GPIO_I2S_SDIN =       (GPIO_NUM_10),
    .GPIO_I2S_DOUT =       (GPIO_NUM_8),
    .CODEC_I2C_ADDR = 0,
    .AUDIO_ADC_I2C_ADDR = 0,

    .IMU_I2C_ADDR = 0,

    /*!< PERI_PWR_ON would be GPIO37, but GPIO37 is reserved (Octal PSRAM/Flash),
     *   so the peripheral-rail power control is disabled on this board. */
    .FUNC_PWR_CTRL =       (0),
    .GPIO_PWR_CTRL =       (GPIO_NUM_NC),
    .GPIO_PWR_ON_LEVEL =   (1),

    /*!< No mute button on this board */
    .GPIO_MUTE_NUM =   GPIO_NUM_NC,
    .GPIO_MUTE_LEVEL = 1,

    .PMOD1 = NULL,
    .PMOD2 = NULL,

    .FUNC_ADC_EN =   (1),
    .adc_tb =        g_adc,
    .adc_len =       sizeof(g_adc) / sizeof(g_adc[0]),

    .FUNC_UART_EN =  (1),
    .uart_param_tb = g_uart,
    .uart_len =      sizeof(g_uart) / sizeof(g_uart[0]),
};

static const char *TAG = "board";

esp_err_t bsp_board_s3_dual_module_init(void)
{
    /**
     * @brief Power up peripheral and audio power domains first.
     *
     * @note The peripheral rail (PERI_PWR_ON) also feeds the LCD, so it must be
     *       enabled before initializing the display. I2C is already brought up
     *       in bsp_board_detect() prior to this call.
     */
    ESP_ERROR_CHECK(bsp_board_s3_dual_module_power_ctrl(POWER_MODULE_ALL, true));

    /*!< Buttons (no-op here since BUTTON_TAB_LEN == 0, kept for completeness) */
    bsp_btn_init_default();

    /*!< ADC (battery voltage measurement on GPIO6 / ADC1_CHANNEL_5) */
    ESP_ERROR_CHECK(bsp_adc_init_default());

    /*!< UART (UART1 on TX=GPIO3 / RX=GPIO21) */
    bsp_uart_default_init();

    /*!< LCD (ST7789 over SPI) */
    ESP_ERROR_CHECK(bsp_lcd_init());

    /**
     * @brief Initialize I2S and audio codec
     *
     * @note Actually the sampling rate can be reconfigured.
     *       `MP3GetLastFrameInfo` can fill the `MP3FrameInfo`, which includes `samprate`.
     *       So theoretically, the sampling rate can be dynamically changed according to the MP3 frame information.
     */
    ESP_ERROR_CHECK(bsp_i2s_init(I2S_NUM_0, 16000));
    ESP_ERROR_CHECK(bsp_codec_init(AUDIO_HAL_16K_SAMPLES));

    return ESP_OK;
}

esp_err_t bsp_board_s3_dual_module_power_ctrl(power_module_t module, bool on)
{
    /* Config power control IO: only PA_CTRL (GPIO48).
     * The peripheral power rail (PERI_PWR_ON) is not controllable on this board
     * because its pin (GPIO37) is reserved for Octal PSRAM/Flash. */
    static esp_err_t bsp_io_config_state = ESP_FAIL;
    if (ESP_OK != bsp_io_config_state) {
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << GPIO_PA_CTRL);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        bsp_io_config_state = gpio_config(&io_conf);
    }

    /* Check IO config result */
    if (ESP_OK != bsp_io_config_state) {
        ESP_LOGE(TAG, "Failed initialize power control IO");
        return bsp_io_config_state;
    }

    /* Control independent power domain */
    switch (module) {
    case POWER_MODULE_LCD:
        /* No dedicated LCD power rail control on this board */
        break;
    case POWER_MODULE_AUDIO:
    case POWER_MODULE_ALL:
        /* Audio power amplifier enable */
        gpio_set_level(GPIO_PA_CTRL, on ? (GPIO_PA_CTRL_ON) : (!GPIO_PA_CTRL_ON));
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

const board_res_desc_t *bsp_board_dual_module_get_res_desc(void)
{
    return &g_board_s3_dual_module_res;
}
