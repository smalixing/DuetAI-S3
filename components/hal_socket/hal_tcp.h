#ifndef __HAL_TCP_H__
#define __HAL_TCP_H__

#include <stdint.h>

int hal_tcp_connect(const char *host, uint16_t port, uint32_t timeout_ms);
int hal_tcp_disconnect(int fd);
int hal_tcp_write(int fd, const unsigned char *buf, uint32_t len, uint32_t timeout_ms);
int hal_tcp_read(int fd, unsigned char *buf, uint32_t len, uint32_t timeout_ms);
int hal_tcp_read_len(int fd, unsigned char *buf, uint32_t len, uint32_t timeout_ms);

#endif
