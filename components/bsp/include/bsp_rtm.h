#ifndef _BSP_RTM_H
#define _BSP_RTM_H
typedef enum
{
    BSP_RTM_PROTOCOL_NEC = 0,
    BSP_RTM_PROTOCOL_RC5,
}BSP_RTM_PROTOCOL_T;
void bsp_rmt_default_init();
int rmt_tx_init(uint8_t gpio_num,uint8_t rmt_channel);
int bsp_rmt_write(uint8_t id,BSP_RTM_PROTOCOL_T protocal,uint32_t addr,uint32_t cmd);

void bsp_rmt_ir_set_freq(uint8_t id, int freq);
void bsp_rmt_ir_code_send(uint8_t id, uint16_t *buf, int len);

#endif // !_BSP_RTM_H