idf_component_register(
    SRCS "main.c" "spiffs_audio.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_joyinside
        nvs_flash
        esp_event
        esp_netif
        spiffs
        protocol_examples_common
)

spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
