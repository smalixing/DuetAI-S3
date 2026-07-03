#ifndef _BSP_ADC_H
#define _BSP_ADC_H

esp_err_t bsp_adc_init_default(void);
uint32_t bsp_adc_get_volatge(uint8_t adc_unit,uint8_t channel);


#endif // !_BSP_ADC_H