#include <stdint.h>
#include "esp_check.h"
#include "esp_log.h"
#include "bsp_board.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_err.h"
#include "bsp_adc.h"
//ADC Calibration
#if CONFIG_IDF_TARGET_ESP32
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_VREF
#elif CONFIG_IDF_TARGET_ESP32S2
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32C3
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32S3
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_TP_FIT
#endif
#define DEFAULT_VREF    1100
static esp_adc_cal_characteristics_t adc1_chars;
static esp_adc_cal_characteristics_t adc2_chars;
static const char *TAG = "ADC SINGLE";


static bool adc_calibration_init(void)
{
    esp_err_t ret;
    bool cali_enable = false;

    ret = esp_adc_cal_check_efuse(ADC_EXAMPLE_CALI_SCHEME);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Calibration scheme not supported, skip software calibration");
    } else if (ret == ESP_ERR_INVALID_VERSION) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else if (ret == ESP_OK) {
        cali_enable = true;
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, DEFAULT_VREF, &adc1_chars);
        esp_adc_cal_characterize(ADC_UNIT_2, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, DEFAULT_VREF, &adc2_chars);
    } else {
        ESP_LOGE(TAG, "Invalid arg");
    }

    return cali_enable;
}

esp_err_t bsp_adc_init_default(void)
{
    
    const board_res_desc_t *brd = bsp_board_get_description();
    adc_calibration_init();
    for(uint8_t i = 0;i<brd->adc_len;i++)
    {
        if(brd->adc_tb[i].adc_unit == ADC_UNIT_1)
        {
            ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
            ESP_ERROR_CHECK(adc1_config_channel_atten(brd->adc_tb[i].adc_channel, ADC_ATTEN_DB_11));
        }
        else
        {
            ESP_ERROR_CHECK(adc2_config_channel_atten(brd->adc_tb[i].adc_channel, ADC_ATTEN_DB_11));
        }

    }
    return ESP_OK;
}

uint32_t bsp_adc_get_volatge(uint8_t adc_unit,uint8_t channel)
{
    uint32_t adc_reading = 0;
    int temp = 0;
    esp_err_t ret = ESP_OK;
    //Multisampling
    for (int i = 0; i < 10; i++) {
        if(adc_unit == ADC_UNIT_1)
        {
            adc_reading += adc1_get_raw(channel);
        }
        else
        {
            ret = adc2_get_raw(channel, ADC_WIDTH_BIT_DEFAULT, &temp);
            adc_reading += temp;
        }
        
    }
    adc_reading /= 10;
    //Convert adc_reading to voltage in mV
    uint32_t voltage = adc_reading; //esp_adc_cal_raw_to_voltage(adc_reading, &g_button.adc_chars);
    ESP_LOGV(TAG, "Raw: %d\tVoltage: %dmV", adc_reading, voltage);
    return voltage;
}

