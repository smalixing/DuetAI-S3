#include <string.h>
#include <stdlib.h>
#include "hal_log.h"

int url_parse(const char *url, char *scheme, int max_scheme_len, char *host, int max_host_len, int *port, char *path, int max_path_len)
{
    const char *scheme_end_ptr = strstr(url, "://");
    const char *host_ptr;
    if (scheme_end_ptr == NULL) {
        scheme[0] = 0;
        host_ptr = url;
    } else {
        int scheme_len = scheme_end_ptr - url;
        if (scheme_len >= max_scheme_len) {
            hal_log_err("scheme_len is too small, need(%d), real(%d)", scheme_len + 1, max_scheme_len);
            return -1;
        }
        memcpy(scheme, url, scheme_len);
        scheme[scheme_len] = 0;
        host_ptr = scheme_end_ptr + 3;
    }

    const char *path_ptr = strchr(host_ptr, '/');
    int host_len;
    if (path_ptr == NULL) {
        host_len = strlen(host_ptr);
        path[0] = '/';
        path[1] = 0;
    } else {
        host_len = path_ptr - host_ptr;
        char *fragment_ptr = strchr(path_ptr, '#');
        int path_len;
        if (fragment_ptr == NULL) {
            path_len = strlen(path_ptr);
        } else {
            path_len = fragment_ptr - path_ptr;
        }
        if (path_len >= max_path_len) {
            hal_log_err("path_len is too small, need(%d), real(%d)", path_len + 1, max_path_len);
            return -1;
        }
        memcpy(path, path_ptr, path_len);
        path[path_len] = 0;
    }

    if (host_len >= max_host_len) {
        hal_log_err("host_len is too small, need(%d), real(%d)", host_len + 1, max_host_len);
        return -1;
    }
    memcpy(host, host_ptr, host_len);
    host[host_len] = 0;
    char *port_ptr = strchr(host, ':');

    if (port_ptr) {
        *port_ptr = 0;
        *port = atoi(port_ptr + 1);
    } else {
        *port = 0;
    }

    return 0;
}
