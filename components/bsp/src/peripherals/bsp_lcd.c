/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "string.h"
#include "bsp_lcd.h"
#include "bsp_ledc.h"
#include "bsp_board.h"
#include "esp_compiler.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "soc/soc_memory_layout.h"
#include "esp_intr_types.h"

static const char *TAG = "bsp_lcd";

static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;

static void *p_user_data = NULL;
static bool (*p_on_trans_done_cb)(void *) = NULL;
static SemaphoreHandle_t bsp_lcd_flush_done_sem = NULL;

static void bsp_lcd_backlight_init(void)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if (brd->lcd_bl_use_pwm) {
        ESP_LOGI(TAG, "Lcd backlight use pwm ctrl");
    } else {
        ESP_LOGI(TAG, "Lcd backlight use gpio ctrl or NC");
    }
    
    if (brd->GPIO_LCD_BL == GPIO_NUM_NC) {
        ESP_LOGI(TAG, "No lcd backlight io specified");
        return;
    }
    // Prepare and then apply the LEDC PWM timer configuration
    if (brd->lcd_bl_use_pwm) {
        bsp_lcdc_init(brd->GPIO_LCD_BL,brd->LCD_BL_PWM_TIMER,brd->LCD_BL_PWM_TIMER,brd->LCD_BL_PWM_CH);
    } else {
        gpio_set_direction(brd->GPIO_LCD_BL, GPIO_MODE_OUTPUT);
        gpio_set_level(brd->GPIO_LCD_BL, !brd->GPIO_LCD_BL_ON);
    }
}

static esp_err_t bsp_spi_lcd_init(esp_lcd_panel_io_handle_t *p_io_handle, bsp_lcd_trans_cb_t trans_done_cb)
{
    esp_err_t ret_val = ESP_OK;
    const board_res_desc_t *brd = bsp_board_get_description();

    if (NULL == p_io_handle) {
        ESP_LOGE(TAG, "Invalid LCD IO handle");
        return ESP_ERR_INVALID_ARG;
    }

    if (brd->LCD_BUS_WIDTH == 8) {
        spi_bus_config_t buscfg = {
            #ifdef IDF_VERSION_5
            .isr_cpu_id = ESP_INTR_CPU_AFFINITY_1,
            #endif
            .intr_flags = 0,
            .sclk_io_num = brd->GPIO_LCD_CLK,
            .data0_io_num = brd->GPIO_LCD_D00,
            .data1_io_num = brd->GPIO_LCD_D01,
            .data2_io_num = brd->GPIO_LCD_D02,
            .data3_io_num = brd->GPIO_LCD_D03,
            .data4_io_num = brd->GPIO_LCD_D04,
            .data5_io_num = brd->GPIO_LCD_D05,
            .data6_io_num = brd->GPIO_LCD_D06,
            .data7_io_num = brd->GPIO_LCD_D07,
            .flags = SPICOMMON_BUSFLAG_OCTAL,
            .max_transfer_sz = brd->LCD_WIDTH * brd->LCD_HEIGHT * sizeof(uint16_t),
        };
        ret_val |= spi_bus_initialize(brd->LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    } else if (brd->LCD_BUS_WIDTH == 1) {
        spi_bus_config_t buscfg = {
            #ifdef IDF_VERSION_5
            .isr_cpu_id = ESP_INTR_CPU_AFFINITY_1,
            #endif
            .intr_flags = 0,
            .sclk_io_num = brd->GPIO_LCD_CLK,
            .mosi_io_num = brd->GPIO_LCD_DIN,
            .miso_io_num = GPIO_NUM_NC,
            .quadwp_io_num = GPIO_NUM_NC,
            .quadhd_io_num = GPIO_NUM_NC,
            #ifdef LCD_PANEL_USER_3_WIRE_MODE
            .max_transfer_sz = (((brd->LCD_WIDTH * brd->LCD_HEIGHT * sizeof(uint16_t)+1)*9)+7)/8,
            #else
            .max_transfer_sz = brd->LCD_WIDTH * brd->LCD_HEIGHT * sizeof(uint16_t) + 8,
            #endif
        };
        ret_val |= spi_bus_initialize(brd->LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
        //gpio_set_drive_capability(brd->GPIO_LCD_CLK,GPIO_DRIVE_CAP_0);
        //gpio_set_drive_capability(brd->GPIO_LCD_DIN,GPIO_DRIVE_CAP_0);
    }
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = brd->GPIO_LCD_DC,
        .cs_gpio_num = brd->GPIO_LCD_CS,
        .pclk_hz = brd->LCD_FREQ,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = brd->LCD_CMD_BITS,
        .lcd_param_bits = brd->LCD_PARAM_BITS,
        .on_color_trans_done = trans_done_cb,
        .user_ctx = NULL,
    };

    if (brd->LCD_BUS_WIDTH == 8) {
        io_config.flags.octal_mode = 1;
        io_config.spi_mode = 3;
    }
    ret_val |= esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t) brd->LCD_HOST, &io_config, p_io_handle);
    //gpio_set_drive_capability(brd->GPIO_LCD_DC,GPIO_DRIVE_CAP_0);
    //gpio_set_drive_capability(brd->GPIO_LCD_CS,GPIO_DRIVE_CAP_0);
    bsp_lcd_backlight_init();
    bsp_lcd_set_backlight_level(0);
    return ESP_OK;
}

esp_err_t bsp_spi_lcd_deinit(void)
{
    esp_err_t ret_val = ESP_OK;
    const board_res_desc_t *brd = bsp_board_get_description();

    ret_val |= spi_bus_free(brd->LCD_HOST);

    return ret_val;
}

static bool lcd_trans_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *user_data, void *event_data)
{
    (void) panel_io;
    (void) user_data;
    (void) event_data;

    /* Used for `bsp_lcd_flush_wait` */
    if (likely(NULL != bsp_lcd_flush_done_sem)) {
        xSemaphoreGive(bsp_lcd_flush_done_sem);
    }

    /* Call user registered function */
    if (NULL != p_on_trans_done_cb) {
        return p_on_trans_done_cb(p_user_data);
    }

    return false;
}
extern esp_err_t esp_lcd_new_panel_gc9a01(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);
esp_err_t bsp_lcd_init(void)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    bsp_spi_lcd_init(&io_handle, lcd_trans_done_cb);

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = brd->GPIO_LCD_RST,
        .color_space = brd->LCD_COLOR_SPACE,
        .bits_per_pixel = 16,
    };

    if (0 == brd->LCD_DISP_IC_ST) {
        ESP_ERROR_CHECK(esp_lcd_new_panel_nt35510(io_handle, &panel_config, &panel_handle));
    } else if(1 == brd->LCD_DISP_IC_ST) {
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    }
    else{
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
    }

    /**
     * @brief Configure LCD rotation and mirror
     *
     */
    esp_err_t ret_val = ESP_OK;
    ret_val |= esp_lcd_panel_reset(panel_handle);
    ret_val |= esp_lcd_panel_init(panel_handle);
    ret_val |= esp_lcd_panel_invert_color(panel_handle, brd->LCD_COLOR_INV);
    ret_val |= esp_lcd_panel_set_gap(panel_handle, 0, 0);
    ret_val |= esp_lcd_panel_swap_xy(panel_handle, brd->LCD_SWAP_XY);
    ret_val |= esp_lcd_panel_mirror(panel_handle, brd->LCD_MIRROR_X, brd->LCD_MIRROR_Y);
    /* Turn the display on (IDF panel drivers do not enable it in panel_init) */
    ret_val |= esp_lcd_panel_disp_on_off(panel_handle, true);
    char *data = malloc(brd->LCD_HEIGHT*((panel_config.bits_per_pixel+7)/8));
    memset(data,0,brd->LCD_HEIGHT*((panel_config.bits_per_pixel+7)/8));
    for(uint16_t i = 0;i<brd->LCD_WIDTH;i++)
    {
        esp_lcd_panel_draw_bitmap(panel_handle,i,0,(i+1),brd->LCD_HEIGHT,data);
    }
    free(data);
    
    /**
     * @brief Create mutex to receive LCD flush event.
     *
     */
    if (NULL != bsp_lcd_flush_done_sem) {
        ESP_LOGE(TAG, "LCD already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    bsp_lcd_flush_done_sem = xSemaphoreCreateBinary();

    if (NULL == bsp_lcd_flush_done_sem) {
        return ESP_ERR_NO_MEM;
    }

    /* If any function is checking LCD trans status before transmition */
    xSemaphoreGive(bsp_lcd_flush_done_sem);

    return ESP_OK;
}

esp_err_t bsp_lcd_deinit(void)
{
    esp_err_t ret_val = ESP_OK;

    ret_val |= esp_lcd_panel_del(panel_handle);
    ret_val |= esp_lcd_panel_io_del(io_handle);
    ret_val |= bsp_spi_lcd_deinit();

    return ret_val;
}

esp_err_t bsp_lcd_flush(int x1, int y1, int x2, int y2, const void *p_data, TickType_t ticks_to_wait)
{
    /* Wait for previous tansmition done */
    if (pdPASS != xSemaphoreTake(bsp_lcd_flush_done_sem, ticks_to_wait)) {
        return ESP_ERR_TIMEOUT;
    }

    return esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, p_data);
}

esp_err_t bsp_lcd_flush_wait_done(TickType_t ticks_to_wait)
{
    if (pdPASS != xSemaphoreTake(bsp_lcd_flush_done_sem, ticks_to_wait)) {
        return ESP_ERR_TIMEOUT;
    }

    xSemaphoreGive(bsp_lcd_flush_done_sem);

    return ESP_OK;
}

esp_err_t bsp_lcd_set_cb(bool (*trans_done_cb)(void *), void *data)
{
    if (esp_ptr_executable(trans_done_cb)) {
        p_on_trans_done_cb = trans_done_cb;
        p_user_data = data;
    } else {
        ESP_LOGE(TAG, "Invalid function pointer");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t bsp_lcd_set_backlight(bool en)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    return gpio_set_level(brd->GPIO_LCD_BL, en ? brd->GPIO_LCD_BL_ON : !brd->GPIO_LCD_BL_ON);
}

void bsp_lcd_set_backlight_level(int level)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if (brd->lcd_bl_use_pwm) {
        bsp_lcdc_set_level(level,brd->LCD_BL_PWM_CH);
        
    } else {
        gpio_set_level(brd->GPIO_LCD_BL, level ? brd->GPIO_LCD_BL_ON : !brd->GPIO_LCD_BL_ON);
    }
}

uint8_t bsp_lcd_get_backlight_level(void)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    const int ledc_duty_all = 8191;
    uint8_t level;
    if (brd->lcd_bl_use_pwm)
    {
        level = ((ledc_get_duty(LEDC_LOW_SPEED_MODE,brd->LCD_BL_PWM_CH)*100)+(ledc_duty_all%100))/ledc_duty_all;
    }
    else    
    {
        level = gpio_get_level(brd->GPIO_LCD_BL);
    }
    return level;
}
