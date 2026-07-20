idf_component_register(SRCS "main.c" "lvgl_port.c" "lvgl_mem_psram.c" "wake_word_task.c" "audio_player.c"
                            "wifi_sta.c"
                            "joyinside/joyinside.c" "joyinside/joyinside_auth.c"
                            "joyinside/joyinside_protocol.c" "joyinside/joyinside_opus.c"
                       INCLUDE_DIRS "include" "joyinside"
                       REQUIRES bsp lvgl os_hal spi_flash esp-sr spiffs
                                hal_socket espressif__cjson 78__esp-opus
                                esp_wifi esp_netif nvs_flash esp_event esp_timer
                                esp_app_format esp_psram esp_hw_support
                       WHOLE_ARCHIVE)

# Flash the voice/ folder (WAV prompts) into the "voice" SPIFFS partition
spiffs_create_partition_image(voice "${CMAKE_SOURCE_DIR}/voice" FLASH_IN_PROJECT)
