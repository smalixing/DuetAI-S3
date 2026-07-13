set(public_include_dirs "./include")

set(srcs
    "src/joyinside_envelope.c"
    "src/joyinside_auth.c"
    "src/esp_joyinside_bootstrap.c"
    "src/esp_joyinside_chat.c"
    "src/esp_joyinside_chat_event.c"
    "src/esp_joyinside_chat_protocol.c"
)

set(esp_joyinside_priv_requires mbedtls)
if("${IDF_VERSION_MAJOR}" VERSION_LESS "6")
    list(APPEND esp_joyinside_priv_requires json)
endif()

idf_component_register(INCLUDE_DIRS "${public_include_dirs}"
                       SRCS ${srcs}
                       PRIV_INCLUDE_DIRS "src"
                       REQUIRES esp_common esp_websocket_client esp_http_client
                       PRIV_REQUIRES ${esp_joyinside_priv_requires}
                       WHOLE_ARCHIVE)
