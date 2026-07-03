/*
 * @Descripttion: 
 * @version: 
 * @Author: oliver
 * @Date: 2022-11-04 09:41:48
 */
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/select.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "bsp_uart.h"
#include "bsp_board.h"
const char *TAG = "UART";
static int *bsp_uart_fd_tb = NULL;

void bsp_uart_default_init(void)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    bsp_uart_fd_tb = (brd->uart_len >0 && bsp_uart_fd_tb == NULL)?malloc(brd->uart_len*sizeof(int)):NULL;
    for(uint8_t i = 0;i<brd->uart_len;i++)
    {
        ESP_LOGI("uart","rate:%d data bits:%d parity:%d stop bits%d",
        brd->uart_param_tb[i].rate,brd->uart_param_tb[i].data_bits,brd->uart_param_tb[i].parity,brd->uart_param_tb[i].stop_bit);
        int fd = bsp_uart_open(i,brd->uart_param_tb[i].rate,brd->uart_param_tb[i].data_bits,brd->uart_param_tb[i].parity,brd->uart_param_tb[i].stop_bit);
        if(fd != -1)
        {
            bsp_uart_fd_tb[i] = fd;
        }
    }
}

int bsp_uart_open(uint8_t id,uint32_t rate,bsp_uart_data_bits_t data_bits,bsp_uart_parity_t parity,bsp_uart_stop_bits_t stop_bit)
{
    int fd = -1;
    const board_res_desc_t *brd = bsp_board_get_description();
    if(id >= brd->uart_len)
    {
        return -1;
    }
    uart_config_t uart_config = {
        .baud_rate = rate,
        .data_bits = data_bits,
        .parity = parity,
        .stop_bits = stop_bit,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_LOGI("uart","port:%d tx:%d rx:%d en:%d",brd->uart_param_tb[id].uart_port,brd->uart_param_tb[id].tx_gpio_num,
    brd->uart_param_tb[id].rx_gpio_num,brd->uart_param_tb[id].en_gpio_num);
    ESP_ERROR_CHECK(uart_driver_install(brd->uart_param_tb[id].uart_port, 512 * 2, 0, 0, NULL, 0));

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(brd->uart_param_tb[id].uart_port, &uart_config));

    ESP_LOGI(TAG, "UART set pins, mode and install driver.");

    // Set UART pins as per KConfig settings
    ESP_ERROR_CHECK(uart_set_pin(brd->uart_param_tb[id].uart_port,brd->uart_param_tb[id].tx_gpio_num,
    brd->uart_param_tb[id].rx_gpio_num,brd->uart_param_tb[id].en_gpio_num, UART_PIN_NO_CHANGE));

    // Set RS485 half duplex mode
    ESP_ERROR_CHECK(uart_set_mode(brd->uart_param_tb[id].uart_port, brd->uart_param_tb[id].uart_mode));

    ESP_ERROR_CHECK(uart_set_rx_timeout(brd->uart_param_tb[id].uart_port, 3));
    char str[20] = {0};
    sprintf(str,"/dev/uart/%u",brd->uart_param_tb[id].uart_port);
    ESP_LOGI("uart","open:%s",str);
    if ((fd = open(str, O_RDWR)) == -1) {
        ESP_LOGE(TAG, "Cannot open UART");
        return -1;
    }
    esp_vfs_dev_uart_use_driver(brd->uart_param_tb[id].uart_port);
    return fd;
}

int bsp_uart_get_fd(uint8_t id)
{
    int fd = -1;
    const board_res_desc_t *brd = bsp_board_get_description();
    if(id < brd->uart_len)
    {
        fd = bsp_uart_fd_tb[id];
    }
    return fd;
}

int bsp_uart_close(uint8_t id)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if(id < brd->uart_len)
    {
        int fd = bsp_uart_fd_tb[id];
        close(fd);
        bsp_uart_fd_tb[id] = -1;
        return 0;
    }
    return -1;
}

uint16_t bsp_uart_read(int fd,uint8_t *data,uint16_t size)
{
    int len = 0;
    if((len =read(fd,data,size)) > 0)
    {
        return len;
    }
    return 0;
}

int bsp_uart_write(int fd,uint8_t *data,uint16_t len)
{
    return write(fd,data,len);
}

uint16_t bsp_uart_read_by_id(uint8_t id,uint8_t *data,uint16_t size)
{
    int len = 0;
    const board_res_desc_t *brd = bsp_board_get_description();
    if(id >= brd->uart_len)
    {
        return -1;
    }
    if((len =uart_read_bytes(brd->uart_param_tb[id].uart_port, data, size, (100/ portTICK_PERIOD_MS))) > 0)
    {
        return len;
    }
    return 0;
}

int bsp_uart_write_by_id(uint8_t id,uint8_t *data,uint16_t len)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if(id >= brd->uart_len)
    {
        return -1;
    }
    if (uart_write_bytes(brd->uart_param_tb[id].uart_port, data, len) != len)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

int bsp_uart_clear_input_by_id(uint8_t id)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if(id >= brd->uart_len)
    {
        return -1;
    }
    return uart_flush_input(brd->uart_param_tb[id].uart_port);
}
