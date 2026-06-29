#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal_log.h"
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int HAL_UDP_Connect(const char *host, unsigned short port)
{
    int             ret;
    struct addrinfo hints, *addr_list, *cur;
    int             fd = 0;

    char port_str[6] = {0};
    snprintf(port_str, 6, "%d", port);

    memset((char *)&hints, 0x00, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family   = AF_INET;
    hints.ai_protocol = IPPROTO_UDP;

    hal_log_debug("udp connect to [%s:%d]", host, port);

    if (getaddrinfo(host, port_str, &hints, &addr_list) != 0) {
        hal_log_err("getaddrinfo error,errno:%s", strerror(errno));
        return -1;
    }

    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (fd < 0) {
            ret = -1;
            continue;
        }

        if (0 == connect(fd, cur->ai_addr, cur->ai_addrlen)) {
            ret = fd;
            break;
        }

        close(fd);
        ret = -1;
    }

    if (ret < 0) {
        hal_log_err("fail to establish udp");
    } else {
        hal_log_debug("success to establish udp, fd=%d", ret);
    }

    freeaddrinfo(addr_list);
    return ret;
}

void HAL_UDP_Disconnect(int fd)
{
    close(fd);
}

int HAL_UDP_Write(int fd, const unsigned char *p_data, unsigned int datalen)
{
    return send(fd, (char *)p_data, (int)datalen, 0);
}

int HAL_UDP_Read(int fd, unsigned char *p_data, unsigned int datalen)
{
    return (int)read(fd, p_data, datalen);
}

int HAL_UDP_ReadTimeout(int fd, unsigned char *p_data, unsigned int datalen, unsigned int timeout_ms)
{
    fd_set         read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd + 1, &read_fds, NULL, NULL, timeout_ms == 0 ? NULL : &tv);

    /* Zero fds ready means we timed out */
    if (ret == 0) {
        hal_log_info("select timeout!");
        return 0; /* receive timeout */
    }

    if (ret < 0) {
        if (errno == EINTR) {
            hal_log_info("select EINTR be caught");
            return 0; /* want read */
        }

        hal_log_err("select-read error: %s", strerror(errno));
        return -1; /* receive failed */
    }

    /* This call will not block */
    return HAL_UDP_Read(fd, p_data, datalen);
}


int HAL_UDP_CreateBind(const char *host, unsigned short port)
{
    int             ret;
    struct addrinfo hints, *addr_list, *cur;
    int             fd = 0;

    char port_str[6] = {0};
    snprintf(port_str, 6, "%d", port);

    memset((char *)&hints, 0x00, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family   = AF_INET;
    hints.ai_protocol = IPPROTO_UDP;

    hal_log_debug("establish udp connection with server(host=%s port=%s)", host, port_str);

    if (getaddrinfo(host, port_str, &hints, &addr_list) != 0) {
        hal_log_err("getaddrinfo error,errno:%s", strerror(errno));
        return -1;
    }

    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (fd < 0) {
            ret = -1;
            continue;
        }

        int iOptval = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &iOptval, sizeof(int)) < 0) {
            hal_log_err("setsockopt SO_REUSEADDR failed", strerror(errno));
        }
        if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &iOptval, sizeof(int)) < 0) {
            hal_log_err("setsockopt SO_BROADCAST failed", strerror(errno));
        }

        hal_log_debug("establish udp connection with server(host=%d port=%d)", ((struct sockaddr_in *)cur->ai_addr)->sin_addr,
              ((struct sockaddr_in *)cur->ai_addr)->sin_port);

        if (0 == bind(fd, cur->ai_addr, cur->ai_addrlen)) {
            ret = fd;
            break;
        }

        close(fd);
        ret = -1;
    }

    if (ret < 0) {
        hal_log_err("fail to establish udp");
    } else {
        hal_log_info("success to establish udp, fd=%d", ret);
    }

    freeaddrinfo(addr_list);

    hal_log_info("HAL_UDP_Connect fd:%d", ret);

    return ret;
}

int HAL_UDP_ReadTimeoutPeerInfo(int fd, unsigned char *p_data, unsigned int datalen, unsigned int timeout_ms,
                                char *recv_ip_addr, unsigned char recv_addr_len, unsigned short *recv_port)
{
    int                ret;
    struct timeval     tv;
    fd_set             read_fds;
    struct sockaddr_in source_addr;
    socklen_t          addrlen = sizeof(source_addr);
    int                len     = 0;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    ret = select(fd + 1, &read_fds, NULL, NULL, timeout_ms == 0 ? NULL : &tv);

    if (ret == 0) {
        hal_log_info("select timeout!");
        return 0; /* receive timeout */
    }

    if (ret < 0) {
        if (errno == EINTR) {
            hal_log_info("select EINTR be caught");
            return 0; /* want read */
        }

        hal_log_err("select-read error: %s", strerror(errno));
        return -1; /* receive failed */
    }

    /* This call will not block */
    len = recvfrom(fd, p_data, datalen, MSG_DONTWAIT, (struct sockaddr *)&source_addr, &addrlen);

    char *ip = inet_ntoa(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr);
    strcpy(recv_ip_addr, ip);
    *recv_port = ntohs(source_addr.sin_port);

    return len;
}

int HAL_UDP_WriteTo(int fd, const unsigned char *p_data, unsigned int datalen, char *host, unsigned short port)
{
    int             rc = -1;
    struct addrinfo hints, *addr_list;
    char            port_str[6] = {0};

    snprintf(port_str, 6, "%d", port);

    memset((char *)&hints, 0x00, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family   = AF_INET;
    hints.ai_protocol = IPPROTO_UDP;

    if ((rc = getaddrinfo(host, port_str, &hints, &addr_list)) != 0) {
        hal_log_err("getaddrinfo error,errno:%s, rc:%d", strerror(errno), rc);
        return -1;
    }

    hal_log_debug("(host=%d port=%d)", ((struct sockaddr_in *)addr_list->ai_addr)->sin_addr,
            ((struct sockaddr_in *)addr_list->ai_addr)->sin_port);

    rc = sendto(fd, p_data, datalen, 0, addr_list->ai_addr, addr_list->ai_addrlen);

    freeaddrinfo(addr_list);

    return rc;
}

void HAL_UDP_Close(int fd)
{
    close(fd);
}
