cmake_minimum_required(VERSION 3.5)

project (hal_socket)

set(SOURCES
    hal_log.c
    hal_http.c
    hal_network.c
    hal_tcp.c
    hal_tls.c
    hal_websocket.c
    hal_udp.c
    url_parse.c
)

add_library(hal_socket STATIC ${SOURCES})
