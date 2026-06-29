#include "driver/uart.h"

char rt_hw_console_getchar(void)
{
    static int u_init = 0;
    if (!u_init) {
        u_init = 1;
        uart_driver_install(UART_NUM_0, 1024, 1024, 0, NULL, 0);
    }

    char data;
    int len = uart_read_bytes(UART_NUM_0, &data, 1, -1);
    //assert(len == 1);
    return data;
}
