#ifndef __HAL_NETWORK_H__
#define __HAL_NETWORK_H__

#include <stdint.h>
#include "hal_tls.h"

typedef enum {
    NETWORK_TCP = 0,
    NETWORK_TLS = 1,
    NETWORK_UDP = 2,
} NETWORK_TYPE;

typedef struct network network_t;

struct network {
    int (*connect)(network_t *pNetwork, const char *host, int port, uint32_t timeout_ms);
    void (*disconnect)(network_t *pNetwork);
    int (*write)(network_t *pNetwork, const uint8_t *buf, int len, uint32_t timeout_ms);
    int (*read)(network_t *pNetwork, uint8_t *buf, int len, uint32_t timeout_ms);
};

network_t *network_new(NETWORK_TYPE type);
void network_delete(network_t *pNetwork);

int network_read_len(network_t *pNetwork, uint8_t *buf, int len, uint32_t timeout_ms);

void network_tls_set_params(network_t *pNetwork, const tls_param_t *param);


#endif
