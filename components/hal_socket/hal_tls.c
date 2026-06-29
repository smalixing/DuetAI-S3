#include "hal_tcp.h"
#include "hal_tls.h"
#include "hal_log.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"

typedef struct {
    mbedtls_net_context         socket_fd;
    mbedtls_entropy_context     entropy;
    mbedtls_ctr_drbg_context    ctr_drbg;
    mbedtls_ssl_context         ssl;
    mbedtls_ssl_config          ssl_conf;

    mbedtls_x509_crt            ca_cert;
    mbedtls_x509_crt            factory_cert;
    mbedtls_x509_crt            client_cert;
    mbedtls_pk_context          private_key;
} tls_ctx_t;

static void _free_mebedtls(tls_ctx_t *tls_ctx)
{
    mbedtls_net_free(&(tls_ctx->socket_fd));
    mbedtls_x509_crt_free(&(tls_ctx->client_cert));
    mbedtls_x509_crt_free(&(tls_ctx->ca_cert));
    mbedtls_pk_free(&(tls_ctx->private_key));
    mbedtls_ssl_free(&(tls_ctx->ssl));
    mbedtls_ssl_config_free(&(tls_ctx->ssl_conf));
    mbedtls_ctr_drbg_free(&(tls_ctx->ctr_drbg));
    mbedtls_entropy_free(&(tls_ctx->entropy));

    free(tls_ctx);
}

#if defined(MBEDTLS_DEBUG_C)
#define DEBUG_LEVEL 0
static void _ssl_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    hal_log_debug("[%s]:[%d]: %s\r\n", file, line, str);
}
#endif

static int _mbedtls_client_init(tls_ctx_t *tls_ctx, const tls_param_t *tls_param)
{
    int ret = 0;

    mbedtls_net_init(&(tls_ctx->socket_fd));
    mbedtls_ssl_init(&(tls_ctx->ssl));
    mbedtls_ssl_config_init(&(tls_ctx->ssl_conf));
    mbedtls_ctr_drbg_init(&(tls_ctx->ctr_drbg));
    mbedtls_x509_crt_init(&(tls_ctx->ca_cert));
    mbedtls_x509_crt_init(&(tls_ctx->factory_cert));
    mbedtls_x509_crt_init(&(tls_ctx->client_cert));
    mbedtls_pk_init(&(tls_ctx->private_key));

    mbedtls_entropy_init(&(tls_ctx->entropy));

#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
    mbedtls_ssl_conf_dbg(&tls_ctx->ssl_conf, _ssl_debug, NULL);
#endif

    if ((ret = mbedtls_ssl_config_defaults(&(tls_ctx->ssl_conf), MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        hal_log_err("mbedtls_ssl_config_defaults failed returned 0x%04x", ret < 0 ? -ret : ret);
        return ret;
    }

    mbedtls_ssl_conf_authmode(&(tls_ctx->ssl_conf), MBEDTLS_SSL_VERIFY_NONE);

    if ((ret = mbedtls_ctr_drbg_seed(&(tls_ctx->ctr_drbg), mbedtls_entropy_func, &(tls_ctx->entropy), NULL, 0)) != 0) {
        hal_log_err("mbedtls_ctr_drbg_seed failed returned 0x%04x", ret < 0 ? -ret : ret);
        return ret;
    }

    mbedtls_ssl_conf_rng(&(tls_ctx->ssl_conf), mbedtls_ctr_drbg_random, &(tls_ctx->ctr_drbg));

    if ((ret = mbedtls_ssl_setup(&(tls_ctx->ssl), &(tls_ctx->ssl_conf))) != 0) {
        hal_log_err("mbedtls_ssl_setup failed returned 0x%04x", ret < 0 ? -ret : ret);
        return ret;
    }

    mbedtls_ssl_set_bio(&(tls_ctx->ssl), &(tls_ctx->socket_fd), mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

    if (!tls_param)
        return 0;

    if (tls_param->ca_crt != NULL) {
        if ((ret = mbedtls_x509_crt_parse(&(tls_ctx->ca_cert), (const unsigned char *)tls_param->ca_crt, tls_param->ca_crt_len))) {
            hal_log_err("parse ca crt failed returned 0x%04x", ret < 0 ? -ret : ret);
            return ret;
        }
        mbedtls_ssl_conf_authmode(&(tls_ctx->ssl_conf), MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&(tls_ctx->ssl_conf), &(tls_ctx->ca_cert), NULL);
    }

    if (tls_param->dev_crt != NULL && tls_param->private_key != NULL) {
        if ((ret = mbedtls_x509_crt_parse(&(tls_ctx->client_cert), (const unsigned char *)tls_param->dev_crt, tls_param->dev_crt_len)) != 0) {
            hal_log_err("load client cert file failed returned 0x%x", ret < 0 ? -ret : ret);
            return ret;
        }

        if (tls_param->factory_crt != NULL) {
            if ((ret = mbedtls_x509_crt_parse(&(tls_ctx->client_cert), (const unsigned char *)tls_param->factory_crt, tls_param->factory_crt_len)) != 0) {
                hal_log_err("load factory cert file failed returned 0x%x", ret < 0 ? -ret : ret);
                return ret;
            }
        }

#if (MBEDTLS_VERSION_NUMBER >= 0x03000000)
        ret = mbedtls_pk_parse_key(&(tls_ctx->private_key), (const unsigned char *)tls_param->private_key,
                                   tls_param->private_key_len, (const unsigned char *)"", 0, NULL, NULL);
#else
        ret = mbedtls_pk_parse_key(&(tls_ctx->private_key), (const unsigned char *)tls_param->private_key,
                                   tls_param->private_key_len, (const unsigned char *)"", 0);
#endif

        if (ret != 0) {
            hal_log_err("load client key file failed returned 0x%x", ret < 0 ? -ret : ret);
            return ret;
        }

        if ((ret = mbedtls_ssl_conf_own_cert(&(tls_ctx->ssl_conf), &(tls_ctx->client_cert), &(tls_ctx->private_key))) != 0) {
            hal_log_err("mbedtls_ssl_conf_own_cert failed returned 0x%04x", ret < 0 ? -ret : ret);
            return ret;
        }
    } else {
        hal_log_debug("cert_file/key_file is empty!");
    }

    if (tls_param->psk != NULL && tls_param->psk_id != NULL) {
        if ((ret = mbedtls_ssl_conf_psk(&(tls_ctx->ssl_conf), (unsigned char *)tls_param->psk,
                                        tls_param->psk_len, (const unsigned char *)tls_param->psk_id, tls_param->psk_id_len)) != 0) {
            hal_log_err("mbedtls_ssl_conf_psk fail: 0x%x", ret < 0 ? -ret : ret);
            return ret;
        }
        mbedtls_ssl_conf_authmode(&(tls_ctx->ssl_conf), MBEDTLS_SSL_VERIFY_REQUIRED);
    } else {
        hal_log_debug("psk/pskid is empty!");
    }

    return 0;
}

void *hal_tls_connect(const tls_param_t *tls_param, const char *host, int port, uint32_t timeout_ms)
{
    int ret = 0;

    tls_ctx_t *tls_ctx = (tls_ctx_t *)malloc(sizeof(tls_ctx_t));
    if (tls_ctx == NULL) {
        hal_log_err("malloc tls_ctx fail\r\n");
        return 0;
    }
    memset(tls_ctx, 0, sizeof(tls_ctx_t));

    if ((ret = _mbedtls_client_init(tls_ctx, tls_param)) != 0) {
        goto error;
    }

    mbedtls_ssl_conf_read_timeout(&(tls_ctx->ssl_conf), timeout_ms);

    if ((ret = mbedtls_ssl_set_hostname(&(tls_ctx->ssl), host)) != 0) {
        hal_log_err("mbedtls_ssl_set_hostname failed returned 0x%04x", ret < 0 ? -ret : ret);
        goto error;
    }

    if ((ret = hal_tcp_connect(host, port, timeout_ms)) < 0) {
        goto error;
    } else {
        tls_ctx->socket_fd.fd = ret;
    }

    while ((ret = mbedtls_ssl_handshake(&(tls_ctx->ssl))) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            hal_log_err("mbedtls_ssl_handshake failed returned 0x%04x", ret < 0 ? -ret : ret);
            if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
                hal_log_err("Unable to verify the server's certificate");
            }
            goto error;
        }
    }

    if ((ret = mbedtls_ssl_get_verify_result(&(tls_ctx->ssl))) != 0) {
        hal_log_err("mbedtls_ssl_get_verify_result failed returned 0x%04x", ret < 0 ? -ret : ret);
    }

    hal_log_debug("ssl connected success with [%s:%d]", host, port);
    return tls_ctx;

error:
    _free_mebedtls(tls_ctx);
    return 0;
}

void hal_tls_disconnect(void *ctx)
{
    if (NULL == ctx) {
        hal_log_err("ctx is NULL");
        return;
    }
    _free_mebedtls((tls_ctx_t *)ctx);
}

int hal_tls_write(void *ctx, const unsigned char *buf, uint32_t len, uint32_t timeout_ms)
{
    tls_ctx_t *tls_ctx = (tls_ctx_t *)ctx;

    int fd = tls_ctx->socket_fd.fd;
    fd_set write_sets;
    FD_ZERO(&write_sets);
    FD_SET(fd, &write_sets);

    struct timeval timeout;
    timeout.tv_sec  = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd + 1, NULL, &write_sets, NULL, &timeout);

    if (ret > 0) {
        int ret = mbedtls_ssl_write(&(tls_ctx->ssl), buf, len);
        if (ret < 0)
            hal_log_err("mbedtls_ssl_write failed: 0x%04x", ret < 0 ? -ret : ret);
        return ret;
    } else if (0 == ret) {
        return 0;
    } else {
        if (EINTR != errno) {
            hal_log_err("sock[%d] select error: %s", fd, strerror(errno));
            return -1;
        } else {
            hal_log_info("select EINTR be caught");
            return 0;
        }
    }
}

int hal_tls_read(void *ctx, unsigned char *buf, size_t len, uint32_t timeout_ms)
{
    tls_ctx_t *tls_ctx = (tls_ctx_t *)ctx;

    mbedtls_ssl_conf_read_timeout(&(tls_ctx->ssl_conf), timeout_ms);
    int ret = mbedtls_ssl_read(&(tls_ctx->ssl), buf, len);
    if (ret >= 0)
        return ret;

    if (ret != MBEDTLS_ERR_SSL_TIMEOUT) {
        hal_log_err("mbedtls_ssl_read failed: 0x%04x", ret < 0 ? -ret : ret);
        return ret;
    }

    return 0;
}

int hal_tls_read_len(void *ctx, unsigned char *buf, size_t len, uint32_t timeout_ms)
{
    int ret;
    int recv_len = 0;

    do {
        ret = hal_tls_read(ctx, buf + recv_len, len - recv_len, timeout_ms);
    } while (ret > 0 && (recv_len += ret) < len);

    if (ret < 0)
        return ret;

    return recv_len;
}
