#ifndef __HAL_HTTP_H__
#define __HAL_HTTP_H__

typedef int (*hal_http_recv_cb_t)(void *param, unsigned char *buf, int len, int offset, int content_length);
typedef int (*hal_http_send_cb_t)(void *param, unsigned char *buf, int buf_len, int offset);

int hal_http_download(const char *url, int opt_port, int offset, int size, hal_http_recv_cb_t recv_cb, void *param);

int hal_http_upload(const char *url, int opt_port, int content_length, hal_http_send_cb_t send_cb, void *param);

/**
 * 新增接口：通用HTTP请求
 * @param method HTTP方法 (GET, POST, PUT, DELETE等)
 * @param url 请求URL
 * @param opt_port 可选端口号，0表示使用默认端口
 * @param headers 自定义HTTP头部，可以为NULL。格式："Header1: value1\r\nHeader2: value2\r\n"
 * @param body 请求体数据，可以为NULL
 * @param body_len 请求体长度
 * @param recv_cb 响应数据回调函数，可以为NULL
 * @param param 回调函数参数
 * @return 0成功，-1失败
 */
int hal_http_request(const char *method, const char *url, int opt_port,
                    const char *headers, const char *body, int body_len,
                    hal_http_recv_cb_t recv_cb, void *param);

/**
 * 新增接口：简化的GET请求
 * @param url 请求URL
 * @param opt_port 可选端口号，0表示使用默认端口
 * @param headers 自定义HTTP头部，可以为NULL
 * @param recv_cb 响应数据回调函数，可以为NULL
 * @param param 回调函数参数
 * @return 0成功，-1失败
 */
int hal_http_get(const char *url, int opt_port, const char *headers,
                hal_http_recv_cb_t recv_cb, void *param);

/**
 * 新增接口：简化的POST请求
 * @param url 请求URL
 * @param opt_port 可选端口号，0表示使用默认端口
 * @param headers 自定义HTTP头部，可以为NULL
 * @param body 请求体数据，可以为NULL
 * @param body_len 请求体长度
 * @param recv_cb 响应数据回调函数，可以为NULL
 * @param param 回调函数参数
 * @return 0成功，-1失败
 */
int hal_http_post(const char *url, int opt_port, const char *headers,
                 const char *body, int body_len, hal_http_recv_cb_t recv_cb, void *param);

#endif
