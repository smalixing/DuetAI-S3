#ifndef _BSP_RTM_H
#define _BSP_RTM_H
void bsp_rmt_default_init();
int rmt_tx_init(uint8_t gpio_num,uint8_t rmt_channel);

void bsp_rmt_ir_set_freq(uint8_t id, int freq);
void bsp_rmt_ir_code_send(uint8_t id, uint16_t *buf, int len);

#endif // !_BSP_RTM_H