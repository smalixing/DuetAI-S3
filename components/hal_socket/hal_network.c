#include "hal_network.h"
#include "hal_tcp.h"
#include "hal_tls.h"
#include "hal_udp.h"
#include <stdlib.h>
// #include "aciga/log.h"
#include "hal_log.h"

//tcp
typedef struct {
    network_t par;
    int fd;
} network_tcp_t;

static int network_tcp_connect(network_t *pNetwork, const char *host, int port, uint32_t timeout_ms)
{
    network_tcp_t *p = (network_tcp_t *)pNetwork;
    p->fd = hal_tcp_connect(host, port, timeout_ms);
    if (p->fd < 0) {
        return -1;
    }
    return 0;
}

static int network_tcp_read(network_t *pNetwork, uint8_t *buf, int len, uint32_t timeout_ms)
{
    network_tcp_t *p = (network_tcp_t *) pNetwork;
    return hal_tcp_read(p->fd, buf, len, timeout_ms);
}

static int network_tcp_write(network_t *pNetwork, const uint8_t *buf, int len, uint32_t timeout_ms)
{
    network_tcp_t *p = (network_tcp_t *) pNetwork;
    return hal_tcp_write(p->fd, buf, len, timeout_ms);
}

static void network_tcp_disconnect(network_t *pNetwork)
{
    network_tcp_t *p = (network_tcp_t *) pNetwork;
    if (p->fd >= 0) {
        hal_tcp_disconnect(p->fd);
        p->fd = -1;
    }
}

// udp
typedef struct {
    network_t par;
    int fd;
} network_udp_t;

static int network_udp_connect(network_t *pNetwork, const char *host, int port, uint32_t timeout_ms)
{
    network_udp_t *p = (network_udp_t *)pNetwork;
    p->fd = HAL_UDP_Connect(host, port);
    if (p->fd < 0) {
        return -1;
    }
    return 0;
}

static int network_udp_read(network_t *pNetwork, uint8_t *buf, int len, uint32_t timeout_ms)
{
    network_udp_t *p = (network_udp_t *) pNetwork;
    return HAL_UDP_ReadTimeout(p->fd, buf, len, timeout_ms);
}

static int network_udp_write(network_t *pNetwork, const uint8_t *buf, int len, uint32_t timeout_ms)
{
    network_udp_t *p = (network_udp_t *) pNetwork;
    return HAL_UDP_Write(p->fd, buf, len);
}

static void network_udp_disconnect(network_t *pNetwork)
{
    network_udp_t *p = (network_udp_t *) pNetwork;
    HAL_UDP_Disconnect(p->fd);
    p->fd = -1;
}


// tls
typedef struct {
    network_t par;
    void *ctx;
    const tls_param_t *params;
} network_tls_t;

static int network_tls_connect(network_t *pNetwork, const char *host, int port, uint32_t timeout_ms)
{
    network_tls_t *p = (network_tls_t *)pNetwork;
    p->ctx = hal_tls_connect(p->params, host, port, timeout_ms);
    if (p->ctx == NULL) {
        return -1;
    }
    return 0;
}

static int network_tls_read(network_t *pNetwork, uint8_t *buf, int len, uint32_t timeout_ms)
{
    network_tls_t *p = (network_tls_t *)pNetwork;
    return hal_tls_read(p->ctx, buf, len, timeout_ms);
}

static int network_tls_write(network_t *pNetwork, const uint8_t *buf, int len, uint32_t timeout_ms)
{
    network_tls_t *p = (network_tls_t *)pNetwork;
    return hal_tls_write(p->ctx, buf, len, timeout_ms);
}

static void network_tls_disconnect(network_t *pNetwork)
{
    network_tls_t *p = (network_tls_t *)pNetwork;
    if (p->ctx) {
        hal_tls_disconnect(p->ctx);
        p->ctx = NULL;
    }
}



network_t *network_new(NETWORK_TYPE type)
{
    switch (type) {

    case NETWORK_TCP: {
        network_tcp_t *p       = (network_tcp_t *)calloc(1, sizeof(network_tcp_t));
		if(!p)
		{
			hal_log_err("malloc p fail\r\n");
			return NULL;
		}		
        network_t *pNetwork    = (network_t *)p;
        pNetwork->connect      = network_tcp_connect;
        pNetwork->read         = network_tcp_read;
        pNetwork->write        = network_tcp_write;
        pNetwork->disconnect   = network_tcp_disconnect;
        p->fd                  = -1;
        return pNetwork;
    }

    case NETWORK_TLS: {
        network_tls_t *p       = (network_tls_t *)calloc(1, sizeof(network_tls_t));
		if(!p)
		{
			hal_log_err("malloc p fail\r\n");
			return NULL;
		}		
        network_t *pNetwork    = (network_t *)p;
        pNetwork->connect      = network_tls_connect;
        pNetwork->read         = network_tls_read;
        pNetwork->write        = network_tls_write;
        pNetwork->disconnect   = network_tls_disconnect;
        return pNetwork;
    }

    case NETWORK_UDP: {
        network_udp_t *p       = (network_udp_t *)calloc(1, sizeof(network_udp_t));
		if(!p)
		{
			hal_log_err("malloc p fail\r\n");
			return NULL;
		}			
        network_t *pNetwork    = (network_t *)p;
        pNetwork->connect      = network_udp_connect;
        pNetwork->read         = network_udp_read;
        pNetwork->write        = network_udp_write;
        pNetwork->disconnect   = network_udp_disconnect;
        p->fd                  = -1;
        return pNetwork;
    }

    default:
        break;
    }

    return NULL;
}

void network_delete(network_t *pNetwork)
{
    free(pNetwork);
}

int network_read_len(network_t *pNetwork, uint8_t *buf, int len, uint32_t timeout_ms)
{
    int ret;
    int recv_len = 0;

    do {
        ret = pNetwork->read(pNetwork, buf + recv_len, len - recv_len, timeout_ms);
    } while (ret > 0 && (recv_len += ret) < len);

    if (ret < 0)
        return ret;

    return recv_len;
}

void network_tls_set_params(network_t *pNetwork, const tls_param_t *param)
{
    network_tls_t *p = (network_tls_t *)pNetwork;
    p->params = param;
}
