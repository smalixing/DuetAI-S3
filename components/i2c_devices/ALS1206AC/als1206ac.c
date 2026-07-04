#include "esp_err.h"
#include "esp_log.h"
#include "bsp_i2c.h"
#include "als1206ac.h"
#include "bsp_gpio.h"

/** @brief asl1206ac register map and function codes */
#define asl1206ac_ADDR            (0x38)



static i2c_bus_device_handle_t asl1206ac_handle = NULL;

static inline esp_err_t asl1206ac_read_byte(uint16_t reg_addr, uint8_t *data)
{
    return i2c_bus_read_byte(asl1206ac_handle, reg_addr,data);
}

static inline esp_err_t asl1206ac_read_bytes(uint16_t reg_addr, size_t data_len, uint8_t *data)
{
    return i2c_bus_read_bytes(asl1206ac_handle, reg_addr, data_len, data);
}

static inline esp_err_t asl1206ac_write_byte(uint16_t reg_addr, uint8_t data)
{
    if(i2c_bus_write_byte(asl1206ac_handle, reg_addr,data) == ESP_OK)
    {
        return ESP_OK;
    }
    else
    {
        ESP_LOGE("als","write byte fail");
        return ESP_FAIL;
    }
}


int asl1206ac_init(void)
{
    if (NULL != asl1206ac_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    bsp_i2c_add_device(I2C_NUM_1,&asl1206ac_handle, asl1206ac_ADDR);
    if (NULL == asl1206ac_handle) {
        ESP_LOGE("asl","i2c add device err");
        return ESP_FAIL;
    }

    esp_err_t ret_val = ESP_OK;
    
    asl1206ac_write_byte(0x00,1);
    asl1206ac_write_byte(0x01,0);
    asl1206ac_write_byte(0x0B,0);
    asl1206ac_write_byte(0x05,0x03);
    asl1206ac_write_byte(0x04,0x08);       // 增益X64倍

    return ESP_OK;
}

int asl1206ac_read_data_status(void)
{
    uint8_t data = 0;
    int err;
    if(ESP_OK == (err = asl1206ac_read_byte(0x17,&data)))
    {
        //ESP_LOGI("als","read status:%d",data);
        if(!(data&0x82))
        {
            err = ESP_FAIL;
        }
    }
    return err;
}

int asl1206ac_read_als(uint16_t *als)
{
    uint8_t als_data_l,als_data_h;
    int err = ESP_FAIL;
    if(ESP_OK == asl1206ac_read_data_status())
    {
        if(ESP_OK == asl1206ac_read_byte(0x1e,&als_data_l)
        && ESP_OK == asl1206ac_read_byte(0x1f,&als_data_h))
        {
            //ESP_LOGI("als","h:%x l:%x",als_data_h,als_data_l);
            *als = (((uint16_t)(als_data_h) << 8) | als_data_l);
            err = ESP_OK;
        }
    }
    return err;
}