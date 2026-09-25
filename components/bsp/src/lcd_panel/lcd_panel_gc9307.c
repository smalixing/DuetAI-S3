#include <stdlib.h>
#include <sys/cdefs.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_board.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_idf_version.h"
#ifdef LCD_PANEL_USER_3_WIRE_MODE
#define lcd_panel_io_tx_param esp_lcd_panel_io_tx_param_9bit
#define lcd_panel_io_tx_color esp_lcd_panel_io_tx_color_9bit
#else
#define lcd_panel_io_tx_param esp_lcd_panel_io_tx_param
#define lcd_panel_io_tx_color esp_lcd_panel_io_tx_color
#endif

static const char *TAG = "lcd_panel.gc9307";
static uint8_t *gc9307_map = NULL;

static esp_err_t panel_gc9307_del(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9307_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9307_init(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9307_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_gc9307_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_gc9307_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_gc9307_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_gc9307_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_gc9307_disp_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    unsigned int bits_per_pixel;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_cal; // save surrent value of LCD_CMD_COLMOD register
} gc9307_panel_t;

esp_err_t esp_lcd_new_panel_gc9307(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    gc9307_panel_t *gc9307 = NULL;
    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    gc9307 = calloc(1, sizeof(gc9307_panel_t));
    ESP_GOTO_ON_FALSE(gc9307, ESP_ERR_NO_MEM, err, TAG, "no mem for gc9307 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->color_space) {
    case ESP_LCD_COLOR_SPACE_RGB:
        gc9307->madctl_val = 0;
        break;
    case ESP_LCD_COLOR_SPACE_BGR:
        gc9307->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16:
        gc9307->colmod_cal = 0x55;
        break;
    case 18:
        gc9307->colmod_cal = 0x66;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    gc9307->io = io;
    gc9307->bits_per_pixel = panel_dev_config->bits_per_pixel;
    gc9307->reset_gpio_num = panel_dev_config->reset_gpio_num;
    gc9307->reset_level = panel_dev_config->flags.reset_active_high;
    gc9307->base.del = panel_gc9307_del;
    gc9307->base.reset = panel_gc9307_reset;
    gc9307->base.init = panel_gc9307_init;
    gc9307->base.draw_bitmap = panel_gc9307_draw_bitmap;
    gc9307->base.invert_color = panel_gc9307_invert_color;
    gc9307->base.set_gap = panel_gc9307_set_gap;
    gc9307->base.mirror = panel_gc9307_mirror;
    gc9307->base.swap_xy = panel_gc9307_swap_xy;
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    gc9307->base.disp_on_off = panel_gc9307_disp_off;
    #else
    gc9307->base.disp_off = panel_gc9307_disp_off;
    #endif
    *ret_panel = &(gc9307->base);
    #ifdef LCD_PANEL_USER_3_WIRE_MODE
    gc9307_map = heap_caps_malloc((((320*240*2+1)*9)+7)/8,MALLOC_CAP_SPIRAM);
    memset(gc9307_map,0,((((320*240*2+1)*9)+7)/8));
    #endif
    ESP_LOGD(TAG, "new gc9307 panel @%p", gc9307);

    return ESP_OK;

err:
    if (gc9307) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(gc9307);
    }
    return ret;
}

static esp_err_t panel_gc9307_del(esp_lcd_panel_t *panel)
{
    gc9307_panel_t *gc9307 = __containerof(panel, gc9307_panel_t, base);

    if (gc9307->reset_gpio_num >= 0) {
        gpio_reset_pin(gc9307->reset_gpio_num);
    }
    ESP_LOGI(TAG, "del gc9307 panel @%p", gc9307);
    free(gc9307);
    return ESP_OK;
}

static esp_err_t panel_gc9307_reset(esp_lcd_panel_t *panel)
{
    gc9307_panel_t *gc9307 = __containerof(panel, gc9307_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9307->io;
    // perform hardware reset
    if (gc9307->reset_gpio_num >= 0) {
        gpio_set_level(gc9307->reset_gpio_num, !gc9307->reset_level);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(gc9307->reset_gpio_num, gc9307->reset_level);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(gc9307->reset_gpio_num, !gc9307->reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else { // perform software reset
        esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5m before sending new command
    }

    return ESP_OK;
}

uint32_t lcd_panel_data_pack_9bit(uint8_t *in,uint32_t ilen,uint8_t *out,bool dc)
{
    uint32_t bit_index = 0;
    uint16_t temp = 0;
    for(uint32_t i = 0;i<ilen;i++)
    {
        temp = ((dc << 8)&0xff00) | (in[i]);
        temp = temp << (7-(bit_index%8));   
        out[(bit_index/8)]  |= ((uint8_t *)(&temp))[1];
        out[(bit_index/8)+1]  |= ((uint8_t *)(&temp))[0];
        bit_index += 9;
    }
    return (bit_index+7)/8;
}

void esp_lcd_panel_io_tx_param_9bit(esp_lcd_panel_io_handle_t io, int lcd_cmd, const uint8_t *param, size_t param_size)
{
    int cmd = 0;
    size_t len = 0;
    lcd_panel_data_pack_9bit((uint8_t *)(&lcd_cmd),1,(uint8_t *)(&cmd),0);
    cmd = ((cmd>>8)&0xff) | ((cmd << 8)&0xff00);
    if(param_size > 0 && gc9307_map != NULL)
    {
        memset(gc9307_map,0,((param_size+1)*9+7)/8);
        len = lcd_panel_data_pack_9bit((uint8_t *)param,param_size,gc9307_map,1);
        esp_lcd_panel_io_tx_param(io,cmd, gc9307_map,len);
        
    }
    else
    {
        esp_lcd_panel_io_tx_param(io,cmd, NULL,0);
    }
}

void esp_lcd_panel_io_tx_color_9bit(esp_lcd_panel_io_handle_t io, int lcd_cmd, uint8_t *param, size_t param_size)
{
    int cmd = 0;
    //size_t len;
    lcd_panel_data_pack_9bit((uint8_t *)(&lcd_cmd),1,(uint8_t *)(&cmd),0);
    cmd = ((cmd>>8)&0xff) | ((cmd << 8)&0xff00);
    if(gc9307_map != NULL)
    {
        memset(gc9307_map,0,((param_size+1)*9+7)/8);
        lcd_panel_data_pack_9bit(param,param_size,gc9307_map,1);
        esp_lcd_panel_io_tx_color(io,cmd, gc9307_map,((param_size+1)*9+7)/8);
    }
}


static esp_err_t panel_gc9307_init(esp_lcd_panel_t *panel)
{
    gc9307_panel_t *gc9307 = __containerof(panel, gc9307_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9307->io;
    // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
    lcd_panel_io_tx_param(io, 0xfe, NULL, 0);
    lcd_panel_io_tx_param(io, 0xef, NULL, 0);
    lcd_panel_io_tx_param(io, 0x86, (uint8_t[]) {
        0x98,
    }, 1);
    lcd_panel_io_tx_param(io, 0x89, (uint8_t[]) {
        0x13,
    }, 1);
    lcd_panel_io_tx_param(io, 0x8b, (uint8_t[]) {
        0x80,
    }, 1);
    lcd_panel_io_tx_param(io, 0x8d, (uint8_t[]) {
        0x33,
    }, 1);

    lcd_panel_io_tx_param(io, 0x8e, (uint8_t[]) {
        0x0f,
    }, 1);

    lcd_panel_io_tx_param(io, 0xe8, (uint8_t[]) {
        0x12,0x70
    }, 2);

	lcd_panel_io_tx_param(io, 0xec, (uint8_t[]) {
        0x33,0x01,0x40
    }, 3);

    lcd_panel_io_tx_param(io, 0xff, (uint8_t[]) {
        0x62
    }, 1);

    lcd_panel_io_tx_param(io, 0x99, (uint8_t[]) {
        0x3e
    }, 1);

    lcd_panel_io_tx_param(io, 0x9d, (uint8_t[]) {
        0x4b
    }, 1);

    lcd_panel_io_tx_param(io, 0x98, (uint8_t[]) {
        0x3e
    }, 1);

    lcd_panel_io_tx_param(io, 0x9c, (uint8_t[]) {
        0x4b
    }, 1);
    lcd_panel_io_tx_param(io, 0xc3, (uint8_t[]) {
        0x0d
    }, 1);
    lcd_panel_io_tx_param(io, 0xc4, (uint8_t[]) {
        0x1d
    }, 1);
    lcd_panel_io_tx_param(io, 0xc9, (uint8_t[]) {
        0x08
    }, 1);
    
    ///gamma
    lcd_panel_io_tx_param(io, 0xF0, (uint8_t[]) {
        0x16,0x17,0x09,0x06,0xf3,0x33
    }, 6);

    lcd_panel_io_tx_param(io, 0xF1, (uint8_t[]) {
        0x49,0x9b,0x9a,0x21,0x23,0xdf
    }, 6);
    

    lcd_panel_io_tx_param(io, 0xF2, (uint8_t[]) {
        0x16,0x17,0x09,0x06,0xf3,0x33
    }, 6);

    lcd_panel_io_tx_param(io, 0xF3, (uint8_t[]) {
        0x49,0x9b,0x9a,0x21,0x23,0xdF
    }, 6);

    lcd_panel_io_tx_param(io, 0xF3, (uint8_t[]) {
        0x49,0x9b,0x9a,0x21,0x23,0xdF
    }, 6);

    lcd_panel_io_tx_param(io, 0x35, (uint8_t[]) {
        0x00
    }, 1);
    lcd_panel_io_tx_param(io, 0x44, (uint8_t[]) {
        0x00,0x0a
    }, 2);

    lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        gc9307->madctl_val,
    }, 1);
    lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]) {
        gc9307->colmod_cal,
    }, 1);

    // turn on display
    //vTaskDelay(pdMS_TO_TICKS(120));
    
    lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0);
    lcd_panel_io_tx_param(io, LCD_CMD_NORON, NULL, 0);
    lcd_panel_io_tx_param(io, LCD_CMD_DISPON, NULL, 0);
    ESP_LOGI(TAG, "%s\r\n",__FUNCTION__);
    return ESP_OK;
}

static esp_err_t panel_gc9307_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    gc9307_panel_t *gc9307 = __containerof(panel, gc9307_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = gc9307->io;

    x_start += gc9307->x_gap;
    x_end += gc9307->x_gap;
    y_start += gc9307->y_gap;
    y_end += gc9307->y_gap;

    // define an area of frame memory where MCU can access
    lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4);
    lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4);
    // transfer frame buffer
    size_t len = ((x_end - x_start) * (y_end - y_start) * (gc9307->bits_per_pixel)+7) / 8;
    lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len);
    //vTaskDelay(pdMS_TO_TICKS(1));
    //ESP_LOGI(TAG, "%s\r\n",__FUNCTION__);
    return ESP_OK;
}

static esp_err_t panel_gc9307_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    gc9307_panel_t *gc9307 = __containerof(panel, gc9307_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9307->io;
    int command = 0;
    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_LOGI(TAG, "%s\r\n",__FUNCTION__);
    lcd_panel_io_tx_param(io, command, NULL, 0);
    return ESP_OK;
}

static esp_err_t panel_gc9307_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    gc9307_panel_t *gc9307 = __containerof(panel, gc9307_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9307->io;
    if (mirror_x) {
        gc9307->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        gc9307->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        gc9307->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        gc9307->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        gc9307->madctl_val
    }, 1);
    ESP_LOGI(TAG, "%s\r\n",__FUNCTION__);
    return ESP_OK;
}

static esp_err_t panel_gc9307_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    gc9307_panel_t *gc9307 = __containerof(panel, gc9307_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9307->io;
    if (swap_axes) {
        gc9307->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        gc9307->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        gc9307->madctl_val
    }, 1);
    ESP_LOGI(TAG, "%s\r\n",__FUNCTION__);
    return ESP_OK;
}

static esp_err_t panel_gc9307_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    gc9307_panel_t *gc9307 = __containerof(panel, gc9307_panel_t, base);
    gc9307->x_gap = x_gap;
    gc9307->y_gap = y_gap;
    ESP_LOGI(TAG, "%s x:%d y:%d\r\n",__FUNCTION__,x_gap,y_gap);
    return ESP_OK;
}

static esp_err_t panel_gc9307_disp_off(esp_lcd_panel_t *panel, bool off)
{
    gc9307_panel_t *gc9307 = __containerof(panel, gc9307_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9307->io;
    int command = 0;
    if (off) {
        command = LCD_CMD_DISPOFF;
    } else {
        command = LCD_CMD_DISPON;
    }
    lcd_panel_io_tx_param(io, command, NULL, 0);
    ESP_LOGI(TAG, "%s cmd:%d\r\n",__FUNCTION__,command);
    return ESP_OK;
}