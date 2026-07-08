#include "hal_websocket.h"
#include "hal_network.h"
#include "hal_log.h"
#include "url_parse.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"


#define WS_FIN                      0x80
#define WS_OPCODE_CONT              0x00
#define WS_OPCODE_TEXT              0x01
#define WS_OPCODE_BINARY            0x02
#define WS_OPCODE_CLOSE             0x08
#define WS_OPCODE_PING              0x09
#define WS_OPCODE_PONG              0x0a
#define WS_OPCODE_CONTROL_FRAME     0x08

#define WS_MASK                     0x80
#define WS_SIZE16                   126
#define WS_SIZE64                   127
#define MAX_WEBSOCKET_HEADER_SIZE   16

#define WS_READ_MAX_TIMEOUT         5000 //ms

#define WS_CLIENT_KEY               "w4v7O6xFTi36lq3RNcgctw=="
#define WS_GUID                     "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


static int _ws_check_accept(const char *response)
{
    const char *p = strcasestr(response, "Sec-WebSocket-Accept:");
    if (p == NULL) {
        hal_log_err("handshake Sec-WebSocket-Accept not found");
        return -1;
    }
    p += strlen("Sec-WebSocket-Accept:");
    while (*p == ' ' || *p == '\t')
        p++;

    /* isolate the field value (up to CR/LF) */
    char server_accept[64];
    int i = 0;
    while (p[i] && p[i] != '\r' && p[i] != '\n' && i < (int)sizeof(server_accept) - 1) {
        server_accept[i] = p[i];
        i++;
    }
    server_accept[i] = 0;

    /* expected = base64(sha1(client_key + GUID)) */
    unsigned char sha1_out[20];
    if (mbedtls_sha1((const unsigned char *)(WS_CLIENT_KEY WS_GUID),
                     strlen(WS_CLIENT_KEY WS_GUID), sha1_out) != 0) {
        hal_log_err("handshake sha1 failed");
        return -1;
    }

    char expected[32];
    size_t olen = 0;
    if (mbedtls_base64_encode((unsigned char *)expected, sizeof(expected), &olen,
                              sha1_out, sizeof(sha1_out)) != 0) {
        hal_log_err("handshake base64 failed");
        return -1;
    }
    expected[olen] = 0;

    if (strcmp(server_accept, expected) != 0) {
        hal_log_err("handshake Accept mismatch, got(%s) want(%s)", server_accept, expected);
        return -1;
    }

    return 0;
}

static int _ws_handshake(network_t *pNetwork, const char *host, int port, char *path, uint32_t timeout_ms)
{
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
                         "GET %s HTTP/1.1\r\n"
                         "Connection: Upgrade\r\n"
                         "host: %s:%d\r\n"
                         "Upgrade: websocket\r\n"
                         "Sec-WebSocket-Version: 13\r\n"
                         "Sec-WebSocket-Key: %s\r\n\r\n", path, host, port, WS_CLIENT_KEY);

    if (len >= sizeof(buf)) {
        hal_log_err("handshake buf size(%d) is too small for header(%d)", sizeof(buf), len);
        return -1;
    }

    int ret = pNetwork->write(pNetwork, (uint8_t *)buf, len, timeout_ms);
    if (ret != len) {
        hal_log_err("handshake write fail, ret(%d), want(%d)", ret, len);
        return -1;
    }

    len = 0;
    do {
        if ((ret = pNetwork->read(pNetwork, (uint8_t *)buf + len, sizeof(buf) - 1 - len, timeout_ms)) <= 0) {
            hal_log_err("handshake read fail, ret(%d)", ret);
            return -1;
        }
        len += ret;
        buf[len] = 0;
        hal_log_debug("handshake recv(%d):\n%s", len, buf);
    } while (NULL == strstr(buf, "\r\n\r\n") && len < sizeof(buf) - 1);

    if (_ws_check_accept(buf) != 0) {
        return -1;
    }

    hal_log_debug("websocket handshake success");
    return 0;
}

void *hal_ws_connect(const char *url, int opt_port, const tls_param_t *tls_param, uint32_t timeout_ms)
{
    char host[128], path[128], scheme[8];
    int port;
    network_t *pNetwork;

    if(0 != url_parse(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path))) {
        hal_log_err("url parse fail! (%s)", url);
        return NULL;
    }

    if (port == 0)
        port = opt_port;

    if (tls_param || !strcmp(scheme, "wss")) {
        pNetwork = network_new(NETWORK_TLS);
        if (pNetwork == NULL) {
            hal_log_err("network_new(TLS) fail");
            return NULL;
        }
        network_tls_set_params(pNetwork, tls_param);
        if (port == 0)
            port = 443;
    } else {
        pNetwork = network_new(NETWORK_TCP);
        if (pNetwork == NULL) {
            hal_log_err("network_new(TCP) fail");
            return NULL;
        }
        if (port == 0)
            port = 80;
    }

    if (pNetwork->connect(pNetwork, host, port, timeout_ms) != 0) {
        hal_log_err("network connect fail");
        goto fail;
    }

    if (_ws_handshake(pNetwork, host, port, path, timeout_ms) != 0) {
        pNetwork->disconnect(pNetwork);
        goto fail;
    }

    return pNetwork;

fail:
    network_delete(pNetwork);
    return NULL;
}

void hal_ws_disconnect(void *ctx)
{
    network_t *pNetwork = (network_t *)ctx;
    pNetwork->disconnect(pNetwork);
    network_delete(pNetwork);
}

static int _ws_write(network_t *pNetwork, int opcode, int mask_flag, const uint8_t *buffer, int len, uint32_t timeout_ms)
{
    uint8_t *packet = (uint8_t *)malloc(len + MAX_WEBSOCKET_HEADER_SIZE);
    if (packet == NULL) {
        hal_log_err("mem oob");
        return -1;
    }

    uint8_t *ws_header = packet;
    int header_len = 0;

    ws_header[header_len++] = opcode;

    if (len <= 125) {
        ws_header[header_len++] = (uint8_t)(len | mask_flag);
    } else if (len < 65536) {
        ws_header[header_len++] = WS_SIZE16 | mask_flag;
        ws_header[header_len++] = (uint8_t)(len >> 8);
        ws_header[header_len++] = (uint8_t)(len & 0xFF);
    } else {
        ws_header[header_len++] = WS_SIZE64 | mask_flag;
        /* Support maximum 4 bytes length */
        ws_header[header_len++] = 0; //(uint8_t)((len >> 56) & 0xFF);
        ws_header[header_len++] = 0; //(uint8_t)((len >> 48) & 0xFF);
        ws_header[header_len++] = 0; //(uint8_t)((len >> 40) & 0xFF);
        ws_header[header_len++] = 0; //(uint8_t)((len >> 32) & 0xFF);
        ws_header[header_len++] = (uint8_t)((len >> 24) & 0xFF);
        ws_header[header_len++] = (uint8_t)((len >> 16) & 0xFF);
        ws_header[header_len++] = (uint8_t)((len >> 8) & 0xFF);
        ws_header[header_len++] = (uint8_t)((len >> 0) & 0xFF);
    }

    if (mask_flag) {
        uint8_t *mask = &ws_header[header_len];

        uint32_t rand_num = rand();
        mask[0] = rand_num >> 24;
        mask[1] = rand_num >> 16;
        mask[2] = rand_num >> 8;
        mask[3] = rand_num >> 0;
        header_len += 4;

        uint8_t *pbuf = packet + header_len;
        for (int i = 0; i < len; ++i)
            pbuf[i] = (buffer[i] ^ mask[i % 4]);
    } else {
        memcpy(packet + header_len, buffer, len);
    }

    int ret = pNetwork->write(pNetwork, packet, header_len + len, timeout_ms);
    free(packet);

    if (ret != header_len + len) {
        hal_log_err("websocket write error, want=%d, ret=%d", header_len + len, ret);
        return -1;
    }

    return 0;
}

int hal_ws_write(void *ctx, const uint8_t *buf, int len, uint32_t timeout_ms)
{
    if (len == 0)
        return 0;

    return _ws_write((network_t *)ctx, WS_OPCODE_BINARY | WS_FIN, WS_MASK, buf, len, timeout_ms);
}

int hal_ws_write_text(void *ctx, const uint8_t *buf, int len, uint32_t timeout_ms)
{
    if (len == 0)
        return 0;

    return _ws_write((network_t *)ctx, WS_OPCODE_TEXT | WS_FIN, WS_MASK, buf, len, timeout_ms);
}

static int _ws_handle_control_frame(network_t *pNetwork, uint32_t timeout_ms, uint32_t payload_len, int opcode)
{
    uint8_t buf[512];
    if (payload_len > sizeof(buf)) {
        hal_log_err("Not enough room for reading control frames (need=%d, max_allowed=%d)", payload_len, sizeof(buf));
        return HAL_WS_ERR;
    }

    if (payload_len) {
        int ret = network_read_len(pNetwork, buf, payload_len, timeout_ms);
        if (ret != payload_len) {
            hal_log_err("Control frame (opcode=%d) payload read failed (payload_len=%d, read_len=%d)", opcode, payload_len, ret);
            return HAL_WS_ERR;
        }
        buf[sizeof(buf) - 1] = 0;
        hal_log_debug("Control frame (opcode=%d) (payload_len=%d):\n%s", opcode, payload_len, buf);
    }

    if (opcode == WS_OPCODE_PING) {
        int ret = _ws_write(pNetwork, WS_OPCODE_PONG | WS_FIN, 0, buf, payload_len, timeout_ms);
        if (ret != 0) {
            hal_log_err("PONG send failed");
            return HAL_WS_ERR;
        }
        hal_log_debug("PONG sent correctly (payload_len=%d)", payload_len);
        return HAL_WS_CONTROL;
    } else if (opcode == WS_OPCODE_CLOSE) {
        // handle CLOSE by the server: send a zero payload frame
        if (payload_len >= 2) {     // if some payload, print out the status code
            uint16_t code = (buf[0] << 8) | buf[1];
            hal_log_info("Got CLOSE frame with status code=%u", code);
        }

        if (_ws_write(pNetwork, WS_OPCODE_CLOSE | WS_FIN, 0, NULL,0, timeout_ms) != 0) {
            hal_log_err("Sending CLOSE frame with 0 payload failed");
            return HAL_WS_ERR;
        }
        hal_log_debug("CLOSE frame with no payload sent correctly");
        return HAL_WS_CLOSED;
    } else if (opcode == WS_OPCODE_PONG) {
        hal_log_debug("Received PONG frame with payload=%d", payload_len);
        return HAL_WS_CONTROL;
    }
    hal_log_info("unkown opcode %d", opcode);
    return HAL_WS_CONTROL;
}

int hal_ws_read(void *ctx, uint8_t *buf, int len, uint32_t timeout_ms)
{
    return hal_ws_read_ex(ctx, buf, len, NULL, timeout_ms);
}

int hal_ws_read_ex(void *ctx, uint8_t *buf, int len, int *opcode, uint32_t timeout_ms)
{
    network_t *pNetwork = (network_t *)ctx;

    uint8_t ws_header[MAX_WEBSOCKET_HEADER_SIZE];
    uint8_t *data_ptr = ws_header;

    int headerLen = 2;
    int ret = network_read_len(pNetwork, ws_header, headerLen, timeout_ms);
    if (ret == 0) {
        return HAL_WS_CLOSED;
    } else if ((ret < 0) || (ret != headerLen)) {
        hal_log_err("read header err, ret=%d", ret);
        return HAL_WS_ERR;
    }

    uint8_t fin = (ws_header[0] >> 7) & 0x01;
    uint8_t frame_opcode = ws_header[0] & 0x0f;
    uint8_t mask = (ws_header[1] >> 7) & 0x01;
    uint32_t payload_len = ws_header[1] & 0x7f;

    data_ptr += headerLen;

    if (payload_len == 126) {
        headerLen = 2;
        ret = network_read_len(pNetwork, data_ptr, headerLen, timeout_ms);
        if (ret != headerLen) {
            hal_log_err("read extend_len(%d) error, ret = %d", headerLen, ret);
            return HAL_WS_ERR;
        }
        payload_len = data_ptr[0] << 8 | data_ptr[1];
    } else if (payload_len == 127) {
        headerLen = 8;
        ret = network_read_len(pNetwork, data_ptr, headerLen, timeout_ms);
        if (ret != headerLen) {
            hal_log_err("read extend_len(%d) error, ret = %d", headerLen, ret);
            return HAL_WS_ERR;
        }
        if (data_ptr[0] != 0 || data_ptr[1] != 0 || data_ptr[2] != 0 || data_ptr[3] != 0) {
            hal_log_err("payload_len is too big");
            return HAL_WS_ERR;
        } else {
            payload_len = (uint32_t)data_ptr[4] << 24 | (uint32_t)data_ptr[5] << 16 | (uint32_t)data_ptr[6] << 8 | data_ptr[7];
        }
    }
    //hal_log_debug("Opcode: %d, mask: %d, len: %d", opcode, mask, payload_len);

    if (mask) {
        hal_log_err("server -> client, mask flag set");
        return HAL_WS_ERR;
    }

    if (frame_opcode & WS_OPCODE_CONTROL_FRAME)
        return _ws_handle_control_frame(pNetwork, WS_READ_MAX_TIMEOUT, payload_len, frame_opcode);

    if (!fin || frame_opcode == WS_OPCODE_CONT)
        hal_log_warn("fragmented frame not supported (fin=%d, opcode=%d), treated as standalone", fin, frame_opcode);

    if (opcode)
        *opcode = frame_opcode;

    uint32_t drop_len = 0;

    if (len < payload_len) {
        drop_len = payload_len - len;
        payload_len = len;
        hal_log_warn("read buf len(%d) too small, drop len %d", len, drop_len);
    }

    if (payload_len) {
        ret = network_read_len(pNetwork, buf, payload_len, WS_READ_MAX_TIMEOUT);
        if (ret != payload_len) {
            hal_log_err("read payload_len err, ret=%d, want=%d", ret, payload_len);
            return HAL_WS_ERR;
        }
    }

    if (drop_len) {
        hal_log_warn("need drop data len = %d", drop_len);
        uint8_t tmp_buf[512];
        while (drop_len) {
            int len = drop_len;
            if (drop_len > sizeof(tmp_buf))
                len = sizeof(tmp_buf);
            ret = network_read_len(pNetwork, tmp_buf, len, WS_READ_MAX_TIMEOUT);
            if (ret != len) {
                hal_log_err("read payload_len(drop) err, ret=%d, want=%d", ret, len);
                break;
            }
            drop_len -= len;
        }
        if (drop_len) {
            hal_log_err("read payload_len(drop) err, remain=%d", drop_len);
            return HAL_WS_ERR;
        }
    }

    return payload_len;
}

int hal_ws_send_close_frame(void *ctx, uint32_t timeout_ms)
{
    int ret = _ws_write((network_t *)ctx, WS_OPCODE_CLOSE | WS_FIN, 0, NULL, 0, timeout_ms);
    if (ret != 0) {
        hal_log_info("ws close frame send fail");
    }
    return ret;
}

int hal_ws_send_ping_frame(void *ctx, uint32_t timeout_ms)
{
    int ret = _ws_write((network_t *)ctx, WS_OPCODE_PING | WS_FIN, 0, NULL, 0, timeout_ms);
    if (ret != 0) {
        hal_log_info("ws ping frame send fail");
    }
    return ret;
}
