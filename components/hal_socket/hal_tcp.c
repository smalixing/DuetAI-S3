#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "hal_log.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netdb.h>

#include <unistd.h>
#include <fcntl.h>

static int tcp_connect(int fd, const struct sockaddr *name, socklen_t namelen, uint32_t timeout_ms)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, NULL) | O_NONBLOCK);

    if (0 == connect(fd, name, namelen)) {
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, NULL) & ~O_NONBLOCK);
        return 0;
    }

    if (errno != EINPROGRESS) {
        hal_log_err("sock[%d] connect error: %s", fd, strerror(errno));
        return -1;
    }

    fd_set write_sets;
    FD_ZERO(&write_sets);
    FD_SET(fd, &write_sets);

    fd_set err_sets;
    FD_ZERO(&err_sets);
    FD_SET(fd, &err_sets);

    struct timeval timeout;
    timeout.tv_sec  = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd + 1, NULL, &write_sets, &err_sets, &timeout);
    if (ret < 0) {
        hal_log_err("sock[%d] select error: %s", fd, strerror(errno));
        return -1;
    } else if (ret == 0) {
        hal_log_err("sock[%d] connect timeout", fd);
        return -1;
    } else {
        if (FD_ISSET(fd, &err_sets)) {
            int sockerr;
            socklen_t len = (socklen_t)sizeof(int);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)(&sockerr), &len);
            hal_log_err("sock[%d] error: %s", fd, strerror(sockerr));
            return -1;
        } else {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, NULL) & ~O_NONBLOCK);
            return 0;
        }
    }
}

int hal_tcp_connect(const char *host, uint16_t port, uint32_t timeout_ms)
{
    int             ret;
    struct addrinfo hints, *addr_list, *cur;
    int             fd = -1;

    char port_str[6];
    snprintf(port_str, 6, "%d", port);

    memset(&hints, 0x00, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    ret = getaddrinfo(host, port_str, &hints, &addr_list);
    if (ret) {
        hal_log_err("getaddrinfo(%s:%s) error %s", host, port_str, strerror(errno));
        return -1;
    }

    ret = -1;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (fd < 0) {
            continue;
        }

        if (tcp_connect(fd, cur->ai_addr, cur->ai_addrlen, timeout_ms) == 0) {
            ret = fd;
            break;
        }

        close(fd);
    }

    if (ret < 0) {
        hal_log_err("failed to connect with TCP server: %s:%s", host, port_str);
    } else {
        hal_log_info("connected with TCP server: %s:%s", host, port_str);
    }

    freeaddrinfo(addr_list);

    return ret;
}

int hal_tcp_disconnect(int fd)
{
    int rc;

#if 0
    /* Shutdown both send and receive operations. */
    rc = shutdown(fd, 2);
    if (0 != rc) {
        hal_log_err("shutdown error: %s", strerror(errno));
        return -1;
    }
#endif

    rc = close(fd);
    if (0 != rc) {
        hal_log_err("closesocket error: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int hal_tcp_write(int fd, const unsigned char *buf, uint32_t len, uint32_t timeout_ms)
{
    fd_set write_sets;
    FD_ZERO(&write_sets);
    FD_SET(fd, &write_sets);

    fd_set err_sets;
    FD_ZERO(&err_sets);
    FD_SET(fd, &err_sets);

    struct timeval timeout;
    timeout.tv_sec  = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd + 1, NULL, &write_sets, &err_sets, &timeout);

    if (ret > 0) {
        if (FD_ISSET(fd, &err_sets)) {
            int sockerr;
            socklen_t len = (socklen_t)sizeof(int);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)(&sockerr), &len);
            hal_log_err("sock[%d] error: %s", fd, strerror(sockerr));
            return -1;
        }
        ret = send(fd, buf, len, 0);
        if (ret < 0) {
            hal_log_err("sock[%d] send err(%s)", fd, strerror(errno));
            if (errno == EINTR)
                return 0;
        }
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

int hal_tcp_read(int fd, unsigned char *buf, uint32_t len, uint32_t timeout_ms)
{
    fd_set read_sets;
    FD_ZERO(&read_sets);
    FD_SET(fd, &read_sets);

    fd_set err_sets;
    FD_ZERO(&err_sets);
    FD_SET(fd, &err_sets);

    struct timeval timeout;
    timeout.tv_sec  = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd + 1, &read_sets, NULL, &err_sets, &timeout);

    if (ret > 0) {
        if (FD_ISSET(fd, &err_sets)) {
            int sockerr;
            socklen_t len = (socklen_t)sizeof(int);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)(&sockerr), &len);
            hal_log_err("sock[%d] error: %s", fd, strerror(sockerr));
            return -1;
        }
        ret = recv(fd, buf, len, 0);
        if (ret < 0) {
            hal_log_err("sock[%d] recv err(%s)", fd, strerror(errno));
            if (errno == EINTR)
                return 0;
        } else if (ret == 0) {
            hal_log_err("sock[%d] recv 0", fd);
            return -1;
        }
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

int hal_tcp_read_len(int fd, unsigned char *buf, uint32_t len, uint32_t timeout_ms)
{
    int len_recv = 0;
    do {
        int ret = hal_tcp_read(fd, buf + len_recv, len - len_recv, timeout_ms);
        if (ret < 0) {
            return ret;
        } else if (ret == 0) {
            break;
        } else {
            len_recv += ret;
        }
    } while (len_recv < len);

    return len_recv;
}
