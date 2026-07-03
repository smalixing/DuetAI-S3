#ifndef _BSP_UART_H
#define _BSP_UART_H
typedef enum {
    BSP_UART_DATA_5_BITS   = 0x0,    /*!< word length: 5bits*/
    BSP_UART_DATA_6_BITS   = 0x1,    /*!< word length: 6bits*/
    BSP_UART_DATA_7_BITS   = 0x2,    /*!< word length: 7bits*/
    BSP_UART_DATA_8_BITS   = 0x3,    /*!< word length: 8bits*/
    BSP_UART_DATA_BITS_MAX = 0x4,
} bsp_uart_data_bits_t;

typedef enum {
    BSP_UART_PARITY_DISABLE  = 0x0,  /*!< Disable UART parity*/
    BSP_UART_PARITY_EVEN     = 0x2,  /*!< Enable UART even parity*/
    BSP_UART_PARITY_ODD      = 0x3   /*!< Enable UART odd parity*/
}bsp_uart_parity_t;

typedef enum {
    BSP_UART_STOP_BITS_1   = 0x1,  /*!< stop bit: 1bit*/
    BSP_UART_STOP_BITS_1_5 = 0x2,  /*!< stop bit: 1.5bits*/
    BSP_UART_STOP_BITS_2   = 0x3,  /*!< stop bit: 2bits*/
    BSP_UART_STOP_BITS_MAX = 0x4,
}bsp_uart_stop_bits_t;

typedef enum{
    BSP_UART_MODE_UART = 0x00,                      /*!< mode: regular UART mode*/
    BSP_UART_MODE_RS485_HALF_DUPLEX = 0x01,         /*!< mode: half duplex RS485 UART mode control by RTS pin */
    BSP_UART_MODE_IRDA = 0x02,                      /*!< mode: IRDA  UART mode*/
    BSP_UART_MODE_RS485_COLLISION_DETECT = 0x03,    /*!< mode: RS485 collision detection UART mode (used for test purposes)*/
    BSP_UART_MODE_RS485_APP_CTRL = 0x04,            /*!< mode: application control RS485 UART mode (used for test purposes)*/
}bsp_uart_mode_t;

void bsp_uart_default_init(void);
int bsp_uart_get_fd(uint8_t id);
int bsp_uart_open(uint8_t id,uint32_t rate,bsp_uart_data_bits_t data_bits,bsp_uart_parity_t parity,bsp_uart_stop_bits_t stop_bit);
uint16_t bsp_uart_read(int fd,uint8_t *data,uint16_t size);
int bsp_uart_write(int fd,uint8_t *data,uint16_t len);
uint16_t bsp_uart_read_by_id(uint8_t id,uint8_t *data,uint16_t size);
int bsp_uart_write_by_id(uint8_t id,uint8_t *data,uint16_t len);
int bsp_uart_clear_input_by_id(uint8_t id);

#endif // !_BSP_UART_

#define _BSP_UART_HH


