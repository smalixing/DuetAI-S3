idf_component_register(SRCS "hal_http.c" "hal_network.c" "hal_tcp.c" "hal_tls.c" "hal_udp.c" "hal_websocket.c" "url_parse.c"
                    INCLUDE_DIRS "."
                    REQUIRES os_hal mbedtls)