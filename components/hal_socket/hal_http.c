#include "hal_http.h"
#include "hal_network.h"
#include "hal_log.h"
#include "url_parse.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define Content_Length      "Content-Length: "
#define Content_Range       "Content-Range: bytes "

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

static int _http_recv_response(network_t *network, char *buf, char **data_ptr, int *data_len)
{
    int len = 0;
    char *end_ptr;
    do {
        int ret = network->read(network, (uint8_t *)buf + len, HTTP_DEFAULT_BUF_SIZE - len, HTTP_DEFAULT_TIMEOUT);
        if (ret <= 0) {
            hal_log_err("http read err or timeout, ret(%d)", ret);
            return -1;
        }
        len += ret;
        buf[len] = 0;
        hal_log_debug("recv:\n%s", buf);
        end_ptr = strstr(buf, "\r\n\r\n");
    } while ((end_ptr == NULL) && (len < HTTP_DEFAULT_BUF_SIZE));

    if (end_ptr == NULL) {
        hal_log_err("recv header err, not find \\r\\n\\r\\n");
        return -1;
    }

    *end_ptr = 0;

    int response_code = atoi(buf + 9);
    hal_log_debug("Response code %d", response_code);

    if (response_code < 200 || response_code >= 300)
        return -1;

    if (data_ptr) {
        *data_ptr = end_ptr + 4;
        *data_len = len - (*data_ptr - buf);
    }

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

    ret = _http_recv_response(network, buf, NULL, NULL);

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

    char *data_ptr;
    int len;
    ret = _http_recv_response(network, buf, &data_ptr, &len);
    if (ret != 0)
        goto exit;

    char *tmp_ptr = strstr(buf, Content_Length);
    if (tmp_ptr == NULL) {
        hal_log_err("not find radical Content-Length");
        ret = -1;
        goto exit;
    }
    int content_len = atoi(tmp_ptr + sizeof(Content_Length) - 1);
    if (size > 0 && content_len + offset != size) {
        hal_log_err("recv Content-Length(%d) offset(%d) not match want(%d)", content_len, offset, size);
        ret = -1;
        goto exit;
    }
    hal_log_debug("content_len = %d", content_len);

    if (offset >= 0) {
        tmp_ptr = strstr(buf, Content_Range);
        if (tmp_ptr == NULL) {
            hal_log_err("not find radical Content-Range, when offset(%d)", offset);
            ret = -1;
            goto exit;
        }
        int range_start = atoi(tmp_ptr + sizeof(Content_Range) - 1);
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
    if (len > 0) {
        user_ret = recv_cb(param, (uint8_t *)data_ptr, len, offset, content_len);
        if (user_ret != 0) {
            hal_log_err(" recv_cb error, exit download!");
            ret = -1;
            goto exit;
        }
        offset += len;
        size -= len;
    }

    while (!user_ret && size > 0) {
        ret = network->read(network, (uint8_t *)buf, HTTP_DEFAULT_BUF_SIZE, HTTP_DEFAULT_TIMEOUT*3); //try 15s to read
        if (ret <= 0) {
            //if (ret == 0) continue;
            hal_log_debug("network read ret(%d), break", ret);
            ret = -1;
            goto exit;
        }
        user_ret = recv_cb(param, (uint8_t *)buf, ret, offset, content_len);
        if (user_ret != 0) {
            hal_log_err(" recv_cb error, exit download!");
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

// 新增：自定义HTTP请求接口的辅助函数
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
    int content_len = (body && body_len > 0) ? body_len : 0;
    ret = _http_send_custom_header(network, buf, method, url, headers, content_len);
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

    // 接收响应
    char *data_ptr;
    int len;
    ret = _http_recv_response(network, buf, &data_ptr, &len);
    if (ret != 0)
        goto exit;

    // 如果提供了回调函数，处理响应数据
    if (recv_cb) {
        int user_ret = 0;
        int offset = 0;
        
        // 解析Content-Length（如果存在）
        int content_len = 0;
        char *tmp_ptr = strstr(buf, Content_Length);
        if (tmp_ptr) {
            content_len = atoi(tmp_ptr + sizeof(Content_Length) - 1);
            hal_log_debug("content_len = %d", content_len);
        }

        // 处理已接收的数据
        if (len > 0) {
            user_ret = recv_cb(param, (uint8_t *)data_ptr, len, offset, content_len);
            offset += len;
            content_len -= len;
        }

        // 继续接收剩余数据
        while (!user_ret && content_len > 0) {
            ret = network->read(network, (uint8_t *)buf, HTTP_DEFAULT_BUF_SIZE, HTTP_DEFAULT_TIMEOUT);
            if (ret <= 0) {
                hal_log_debug("network read ret(%d), break", ret);
                ret = -1;
                goto exit;
            }
            user_ret = recv_cb(param, (uint8_t *)buf, ret, offset, content_len + offset);
            offset += ret;
            content_len -= ret;
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
