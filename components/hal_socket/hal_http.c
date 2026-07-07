#include "hal_http.h"
#include "hal_network.h"
#include "hal_log.h"
#include "url_parse.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CONTENT_LENGTH      "Content-Length: "
#define CONTENT_RANGE       "Content-Range: bytes "
#define CONNECTION_CLOSE    "Connection: close"
#define TRANSFER_ENCODING   "Transfer-Encoding: "

#define HTTP_DEFAULT_TIMEOUT        5000
#define HTTP_DEFAULT_BUF_SIZE       4096

static network_t *_http_connect(const char *url, int opt_port)
{
    char host[128], path[256], scheme[8];
    int port;

    int ret = url_parse(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path));
    if (ret != 0) {
        hal_log_err("url(%s) parse fail", url);
        return NULL;
    }

    hal_log_debug("parse url [%s://%s:%d%s]", scheme, host, port, path);
    if (port == 0)
        port = opt_port;

    network_t *network;
    if (!strcmp(scheme, "http")) {
        network = network_new(NETWORK_TCP);
        if (port == 0)
            port = 80;
    } else {
        network = network_new(NETWORK_TLS);
        if (port == 0)
            port = 443;
    }

    ret = network->connect(network, host, port, HTTP_DEFAULT_TIMEOUT);
    if (ret != 0) {
        hal_log_err("connect to [%s:%d] error!", host, port);
        network_delete(network);
        return NULL;
    }

    return network;
}

static int _http_send_post_header(network_t *network, char *buf, const char *url, int content_len)
{
    char host[128], path[256], scheme[8];
    int port;

    url_parse(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path));
    int len = snprintf(buf, HTTP_DEFAULT_BUF_SIZE,
                "POST %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Content-type: text/html\r\n"
                "Content-length: %d\r\n\r\n"
                , path, host, content_len);

    if (len >= HTTP_DEFAULT_BUF_SIZE) {
        hal_log_err("buf(%d) is too small for POST header(%d)", HTTP_DEFAULT_BUF_SIZE, len);
        return -1;
    }

    hal_log_debug("POST header:\n%s", buf);

    int ret = network->write(network, (uint8_t *)buf, len, HTTP_DEFAULT_TIMEOUT);
    if (ret != len) {
        hal_log_err("send http POST header fail, ret(%d), want(%d)", ret, len);
        return -1;
    }

    return 0;
}

static int _http_send_get_header(network_t *network, char *buf, const char *url, int offset, int size)
{
    char host[128], path[256], scheme[8];
    int port;

    url_parse(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path));
    int len = snprintf(buf, HTTP_DEFAULT_BUF_SIZE,
                "GET %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                //"Accept: application/json;text/html,application/xml;charset=UTF-8\r\n"
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7\r\n"
                "Accept-Encoding: \r\n"
                , path, host);

    if (offset >= 0) {
        if (size > 0) {
            len += snprintf(buf + len, HTTP_DEFAULT_BUF_SIZE - len, "Range: bytes=%d-%d\r\n", offset, offset + size - 1);
        } else {
            len += snprintf(buf + len, HTTP_DEFAULT_BUF_SIZE - len, "Range: bytes=%d-\r\n", offset);
        }
    }

    len += snprintf(buf + len, HTTP_DEFAULT_BUF_SIZE - len, "\r\n");

    if (len >= HTTP_DEFAULT_BUF_SIZE) {
        hal_log_err("buf(%d) is too small for GET header(%d)", HTTP_DEFAULT_BUF_SIZE, len);
        return -1;
    }

    hal_log_debug("GET header:\n%s", buf);

    int ret = network->write(network, (uint8_t *)buf, len, HTTP_DEFAULT_TIMEOUT);
    if (ret != len) {
        hal_log_err("send http GET header fail, ret(%d), want(%d)", ret, len);
        return -1;
    }

    return 0;
}

#include <stdbool.h>

typedef struct {
    int response_code;      /* HTTP状态码 */
    int content_length;     /* Content-Length值，-1表示未指定 */
    bool connection_close;  /* Connection: close */
    bool chunked;           /* Transfer-Encoding: chunked */
    char *body_ptr;         /* buf中body起始位置 */
    int body_len;           /* 随header已收到的body长度 */
} http_response_info_t;

static int _http_recv_header(network_t *network, char *buf, http_response_info_t *info)
{
    int len = 0;
    int ret;
    char *end_ptr = NULL;

    memset(info, 0, sizeof(*info));
    info->content_length = -1;

    /* 1. 读取直到 \r\n\r\n（header结束） */
    while (1) {
        if (len >= HTTP_DEFAULT_BUF_SIZE - 1) {
            hal_log_err("header too large");
            return -1;
        }

        ret = network->read(network,
                            (uint8_t *)buf + len,
                            HTTP_DEFAULT_BUF_SIZE - len - 1,
                            HTTP_DEFAULT_TIMEOUT);

        if (ret <= 0) {
            hal_log_err("http read header failed ret=%d", ret);
            return -1;
        }

        len += ret;
        buf[len] = 0;

        end_ptr = strstr(buf, "\r\n\r\n");
        if (end_ptr) {
            break;
        }
    }

    hal_log_debug("recv(%d) header:\n%s", ret, buf);

    /* 2. 解析状态码 */
    info->response_code = atoi(buf + 9);
    hal_log_debug("response code=%d", info->response_code);

    if (info->response_code < 200 || info->response_code >= 300) {
        return -1;
    }

    /* 3. 解析Content-Length */
    char *ptr = strcasestr(buf, "Content-Length:");
    if (ptr) {
        info->content_length = atoi(ptr + strlen("Content-Length:"));
    }

    /* 4. 解析Connection */
    ptr = strcasestr(buf, "Connection:");
    if (ptr && strcasestr(ptr, "close")) {
        info->connection_close = true;
    }

    /* 5. 解析Transfer-Encoding: chunked */
    ptr = strcasestr(buf, "Transfer-Encoding:");
    if (ptr && strcasestr(ptr, "chunked")) {
        info->chunked = true;
    }

    /* 6. 计算已随header收到的body数据 */
    info->body_ptr = end_ptr + 4;
    info->body_len = len - (info->body_ptr - buf);

    hal_log_debug("content_length=%d, body_len=%d, chunked=%d, close=%d",
                  info->content_length, info->body_len, info->chunked, info->connection_close);

    return 0;
}

static int _http_recv_body_into_buf(network_t *network, char *buf, http_response_info_t *info,
                                    char **data_ptr, int *data_len)
{
    char *body_ptr = info->body_ptr;
    int body_len = info->body_len;

    if (info->content_length >= 0) {
        if (info->content_length > HTTP_DEFAULT_BUF_SIZE - (body_ptr - buf) - 1) {
            hal_log_err("body too large (%d), buf only has %d",
                        info->content_length, HTTP_DEFAULT_BUF_SIZE - (int)(body_ptr - buf) - 1);
            return -1;
        }

        while (body_len < info->content_length) {
            int ret = network->read(network, (uint8_t *)body_ptr + body_len,
                                    info->content_length - body_len, HTTP_DEFAULT_TIMEOUT);
            if (ret <= 0) {
                hal_log_err("read body failed ret = %d", ret);
                return -1;
            }
            body_len += ret;
        }
        body_ptr[body_len] = 0;

        hal_log_debug("recv body len=%d", body_len);

        if (data_ptr) *data_ptr = body_ptr;
        if (data_len) *data_len = body_len;
        return 0;
    }

    if (info->connection_close) {
        while (1) {
            if ((body_ptr - buf) + body_len >= HTTP_DEFAULT_BUF_SIZE - 1) {
                hal_log_err("body buffer full");
                return -1;
            }

            int ret = network->read(network,
                                (uint8_t *)body_ptr + body_len,
                                HTTP_DEFAULT_BUF_SIZE - (body_ptr - buf) - body_len - 1,
                                HTTP_DEFAULT_TIMEOUT);

            if (ret > 0) {
                body_len += ret;
            } else if (ret == 0) {
                break;
            } else {
                if (body_len > 0) {
                    hal_log_warn("read return %d, treat as end (got %d bytes)", ret, body_len);
                    break;
                }
                hal_log_err("read body failed ret=%d", ret);
                return -1;
            }
        }

        body_ptr[body_len] = 0;

        hal_log_debug("recv body(close) len=%d", body_len);

        if (data_ptr) *data_ptr = body_ptr;
        if (data_len) *data_len = body_len;
        return 0;
    }

    if (info->chunked) {
        int buf_cap = HTTP_DEFAULT_BUF_SIZE - (int)(body_ptr - buf) - 1;
        int read_off  = 0;
        int write_off = 0;
        int raw_end   = body_len;

        #define ENSURE_AVAILABLE(need)                                                  \
            do {                                                                        \
                while ((raw_end - read_off) < (need)) {                                 \
                    if (raw_end >= buf_cap) {                                           \
                        hal_log_err("chunked buffer full");                             \
                        return -1;                                                      \
                    }                                                                   \
                    int _r = network->read(network,                                     \
                                           (uint8_t *)body_ptr + raw_end,               \
                                           buf_cap - raw_end,                           \
                                           HTTP_DEFAULT_TIMEOUT);                       \
                    if (_r <= 0) {                                                      \
                        hal_log_err("chunked read failed ret=%d", _r);                  \
                        return -1;                                                      \
                    }                                                                   \
                    raw_end += _r;                                                      \
                }                                                                       \
            } while (0)

        while (1) {
            int line_start = read_off;
            int line_end = -1;
            while (1) {
                ENSURE_AVAILABLE(1);
                if (body_ptr[read_off] == '\r') {
                    ENSURE_AVAILABLE(2);
                    if (body_ptr[read_off + 1] == '\n') {
                        line_end = read_off;
                        read_off += 2;
                        break;
                    }
                }
                read_off++;
            }

            char size_buf[24];
            int line_len = line_end - line_start;
            if (line_len <= 0 || line_len >= (int)sizeof(size_buf)) {
                hal_log_err("invalid chunk size line len=%d", line_len);
                return -1;
            }
            memcpy(size_buf, body_ptr + line_start, line_len);
            size_buf[line_len] = 0;
            for (int i = 0; i < line_len; ++i) {
                if (size_buf[i] == ';' || size_buf[i] == ' ' || size_buf[i] == '\t') {
                    size_buf[i] = 0;
                    break;
                }
            }
            int chunk_size = (int)strtol(size_buf, NULL, 16);
            hal_log_debug("chunk size=%d", chunk_size);

            if (chunk_size < 0) {
                hal_log_err("invalid chunk size=%d", chunk_size);
                return -1;
            }

            if (chunk_size == 0) {
                while (1) {
                    int t_start = read_off;
                    int t_end = -1;
                    while (1) {
                        ENSURE_AVAILABLE(1);
                        if (body_ptr[read_off] == '\r') {
                            ENSURE_AVAILABLE(2);
                            if (body_ptr[read_off + 1] == '\n') {
                                t_end = read_off;
                                read_off += 2;
                                break;
                            }
                        }
                        read_off++;
                    }
                    if (t_end == t_start) {
                        break;
                    }
                }
                break;
            }

            int remain = chunk_size;
            if (write_off + chunk_size > buf_cap) {
                hal_log_err("chunked body too large");
                return -1;
            }

            while (remain > 0) {
                ENSURE_AVAILABLE(1);
                int avail = raw_end - read_off;
                int copy_len = avail < remain ? avail : remain;
                memmove(body_ptr + write_off, body_ptr + read_off, copy_len);
                read_off  += copy_len;
                write_off += copy_len;
                remain    -= copy_len;
            }

            ENSURE_AVAILABLE(2);
            if (body_ptr[read_off] != '\r' || body_ptr[read_off + 1] != '\n') {
                hal_log_err("invalid chunk trailer");
                return -1;
            }
            read_off += 2;
        }

        #undef ENSURE_AVAILABLE

        body_len = write_off;
        body_ptr[body_len] = 0;

        hal_log_debug("recv body(chunked) len=%d", body_len);

        if (data_ptr) *data_ptr = body_ptr;
        if (data_len) *data_len = body_len;
        return 0;
    }

    hal_log_warn("no length/chunked/close header, fallback to read-until-eof");

    while (1) {
        if ((body_ptr - buf) + body_len >= HTTP_DEFAULT_BUF_SIZE - 1) {
            hal_log_err("body buffer full");
            return -1;
        }

        int r = network->read(network,
                            (uint8_t *)body_ptr + body_len,
                            HTTP_DEFAULT_BUF_SIZE - (body_ptr - buf) - body_len - 1,
                            HTTP_DEFAULT_TIMEOUT);

        if (r > 0) {
            body_len += r;
        } else if (r == 0) {
            break;
        } else {
            if (body_len > 0) {
                hal_log_warn("read return %d, treat as end (got %d bytes)", r, body_len);
                break;
            }
            hal_log_err("read body failed ret=%d", r);
            return -1;
        }
    }

    body_ptr[body_len] = 0;

    hal_log_debug("recv body(fallback):\n%s", body_ptr);
    hal_log_debug("recv body(fallback) len=%d", body_len);

    if (data_ptr) *data_ptr = body_ptr;
    if (data_len) *data_len = body_len;
    return 0;
}

int hal_http_upload(const char *url, int opt_port, int content_length, hal_http_send_cb_t send_cb, void *param)
{
    network_t *network = _http_connect(url, opt_port);
    if (network == NULL)
        return -1;

    int ret = -1;

    char *buf = (char *)malloc(HTTP_DEFAULT_BUF_SIZE + 1);
    if (buf == NULL) {
        hal_log_err("mem oob, http buf(%d)", HTTP_DEFAULT_BUF_SIZE);
        goto exit;
    }

    ret = _http_send_post_header(network, buf, url, content_length);
    if (ret != 0)
        goto exit;

    int offset = 0;
    while (1) {
        int len = send_cb(param, (uint8_t *)buf, HTTP_DEFAULT_BUF_SIZE, offset);
        if (len <= 0)
            break;

        ret = network->write(network, (uint8_t *)buf, len, HTTP_DEFAULT_TIMEOUT);
        if (ret != len) {
            hal_log_err("send data err, ret(%d), want(%d)", ret, len);
            ret = -1;
            goto exit;
        }
        offset += len;
        if (offset >= content_length) {
            hal_log_warn("offset(%d) >= content_length(%d)", offset, content_length);
        }
    }

    http_response_info_t resp_info;
    ret = _http_recv_header(network, buf, &resp_info);
    if (ret != 0)
        goto exit;

    ret = 0;

exit:
    free(buf);
    network->disconnect(network);
    network_delete(network);
    return ret;
}

int hal_http_download(const char *url, int opt_port, int offset, int size, hal_http_recv_cb_t recv_cb, void *param)
{
    network_t *network = _http_connect(url, opt_port);
    if (network == NULL)
        return -1;

    int ret = -1;

    char *buf = (char *)malloc(HTTP_DEFAULT_BUF_SIZE + 1);
    if (buf == NULL) {
        hal_log_err("mem oob, http buf(%d)", HTTP_DEFAULT_BUF_SIZE);
        goto exit;
    }

    ret = _http_send_get_header(network, buf, url, offset, size);
    if (ret != 0)
        goto exit;

    /* 只接收并解析HTTP头部 */
    http_response_info_t resp_info;
    ret = _http_recv_header(network, buf, &resp_info);
    if (ret != 0)
        goto exit;

    /* 解析Content-Length */
    if (resp_info.content_length < 0) {
        hal_log_err("not find Content-Length in response header");
        ret = -1;
        goto exit;
    }
    int content_len = resp_info.content_length;
    if (size > 0 && content_len + offset != size) {
        hal_log_err("recv Content-Length(%d) offset(%d) not match want(%d)", content_len, offset, size);
        ret = -1;
        goto exit;
    }
    hal_log_debug("content_len = %d", content_len);

    /* 解析Content-Range（带offset的断点续传请求） */
    if (offset >= 0) {
        char *tmp_ptr = strcasestr(buf, CONTENT_RANGE);
        if (tmp_ptr == NULL) {
            hal_log_err("not find radical Content-Range, when offset(%d)", offset);
            ret = -1;
            goto exit;
        }
        int range_start = atoi(tmp_ptr + sizeof(CONTENT_RANGE) - 1);
        if (range_start != offset) {
            hal_log_err("recv Content-Range(%d) not match want(%d)", range_start, offset);
            ret = -1;
            goto exit;
        }
    }

    size = content_len;
    if (offset < 0)
        offset = 0;

    int user_ret = 0;

    if (resp_info.body_len > 0) {
        user_ret = recv_cb(param, (uint8_t *)resp_info.body_ptr, resp_info.body_len, offset, content_len);
        if (user_ret != 0) {
            hal_log_err("recv_cb error, exit download!");
            ret = -1;
            goto exit;
        }
        offset += resp_info.body_len;
        size -= resp_info.body_len;
    }

    while (!user_ret && size > 0) {
        ret = network->read(network, (uint8_t *)buf, HTTP_DEFAULT_BUF_SIZE, HTTP_DEFAULT_TIMEOUT * 3);
        if (ret <= 0) {
            hal_log_debug("network read ret(%d), break", ret);
            ret = -1;
            goto exit;
        }
        user_ret = recv_cb(param, (uint8_t *)buf, ret, offset, content_len);
        if (user_ret != 0) {
            hal_log_err("recv_cb error, exit download!");
            ret = -1;
            goto exit;
        }
        offset += ret;
        size -= ret;
    };

    ret = 0;

exit:
    free(buf);
    network->disconnect(network);
    network_delete(network);
    return ret;
}

static int _http_send_custom_header(network_t *network, char *buf, const char *method,
                                   const char *url, const char *headers, int content_len)
{
    char host[128], path[256], scheme[8];
    int port;

    url_parse(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path));
    
    // 构建基本的HTTP请求行和Host头
    int len = snprintf(buf, HTTP_DEFAULT_BUF_SIZE,
                "%s %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                , method, path, host);

    // 添加自定义headers（如果提供）
    if (headers && strlen(headers) > 0) {
        len += snprintf(buf + len, HTTP_DEFAULT_BUF_SIZE - len, "%s", headers);
        // 确保headers以\r\n结尾
        if (headers[strlen(headers) - 1] != '\n') {
            len += snprintf(buf + len, HTTP_DEFAULT_BUF_SIZE - len, "\r\n");
        }
    }

    // 如果是POST/PUT等方法且有content_len，添加Content-Length头
    if (content_len > 0) {
        len += snprintf(buf + len, HTTP_DEFAULT_BUF_SIZE - len,
                       "Content-Length: %d\r\n", content_len);
    }

    // 添加结束的空行
    len += snprintf(buf + len, HTTP_DEFAULT_BUF_SIZE - len, "\r\n");

    if (len >= HTTP_DEFAULT_BUF_SIZE) {
        hal_log_err("buf(%d) is too small for %s header(%d)", HTTP_DEFAULT_BUF_SIZE, method, len);
        return -1;
    }

    hal_log_debug("%s header:\n%s", method, buf);

    int ret = network->write(network, (uint8_t *)buf, len, HTTP_DEFAULT_TIMEOUT);
    if (ret != len) {
        hal_log_err("send http %s header fail, ret(%d), want(%d)", method, ret, len);
        return -1;
    }

    return 0;
}

// 新增：通用HTTP请求接口
int hal_http_request(const char *method, const char *url, int opt_port,
                    const char *headers, const char *body, int body_len,
                    hal_http_recv_cb_t recv_cb, void *param)
{
    if (!method || !url) {
        hal_log_err("invalid parameters: method or url is NULL");
        return -1;
    }

    network_t *network = _http_connect(url, opt_port);
    if (network == NULL)
        return -1;

    int ret = -1;

    char *buf = (char *)malloc(HTTP_DEFAULT_BUF_SIZE + 1);
    if (buf == NULL) {
        hal_log_err("mem oob, http buf(%d)", HTTP_DEFAULT_BUF_SIZE);
        goto exit;
    }

    // 发送HTTP请求头
    int req_content_len = (body && body_len > 0) ? body_len : 0;
    ret = _http_send_custom_header(network, buf, method, url, headers, req_content_len);
    if (ret != 0)
        goto exit;

    // 如果有请求体，发送请求体数据
    if (body && body_len > 0) {
        ret = network->write(network, (uint8_t *)body, body_len, HTTP_DEFAULT_TIMEOUT);
        if (ret != body_len) {
            hal_log_err("send body data err, ret(%d), want(%d)", ret, body_len);
            ret = -1;
            goto exit;
        }
    }

    http_response_info_t resp_info;
    ret = _http_recv_header(network, buf, &resp_info);
    if (ret != 0)
        goto exit;

    char *data_ptr = NULL;
    int len = 0;
    ret = _http_recv_body_into_buf(network, buf, &resp_info, &data_ptr, &len);
    if (ret != 0)
        goto exit;

    if (recv_cb == NULL) {
        ret = 0;
        goto exit;
    }

    if (len > 0) {
        int user_ret = recv_cb(param, (uint8_t *)data_ptr, len, 0, len);
        if (user_ret != 0) {
            hal_log_err("recv_cb error, exit request");
            ret = -1;
            goto exit;
        }
    }

    ret = 0;

exit:
    free(buf);
    network->disconnect(network);
    network_delete(network);
    return ret;
}

// 新增：简化的GET请求接口
int hal_http_get(const char *url, int opt_port, const char *headers,
                hal_http_recv_cb_t recv_cb, void *param)
{
    return hal_http_request("GET", url, opt_port, headers, NULL, 0, recv_cb, param);
}

// 新增：简化的POST请求接口
int hal_http_post(const char *url, int opt_port, const char *headers,
                 const char *body, int body_len, hal_http_recv_cb_t recv_cb, void *param)
{
    return hal_http_request("POST", url, opt_port, headers, body, body_len, recv_cb, param);
}
