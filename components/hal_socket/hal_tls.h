#ifndef __HAL_TLS_H__
#define __HAL_TLS_H__

#include <stdint.h>
#include <stddef.h>

typedef struct {
    const char *ca_crt;
    size_t      ca_crt_len;

    const char *factory_crt;
    size_t      factory_crt_len;

    const char *dev_crt;
    size_t      dev_crt_len;
    const char *private_key;
    size_t      private_key_len;
    const char *psk;
    size_t      psk_len;
    const char *psk_id;
    size_t      psk_id_len;
} tls_param_t;

void *hal_tls_connect(const tls_param_t *tls_param, const char *host, int port, uint32_t timeout_ms);
void hal_tls_disconnect(void *ctx);
int hal_tls_write(void *ctx, const unsigned char *buf, uint32_t len, uint32_t timeout_ms);
int hal_tls_read(void *ctx, unsigned char *buf, size_t len, uint32_t timeout_ms);
int hal_tls_read_len(void *ctx, unsigned char *buf, size_t len, uint32_t timeout_ms);

#endif
