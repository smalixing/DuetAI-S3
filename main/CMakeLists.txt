idf_component_register(SRCS "main.c" "lvgl_port.c" "wake_word_task.c" "audio_player.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bsp lvgl os_hal spi_flash esp-sr spiffs)

# Flash the voice/ folder (WAV prompts) into the "voice" SPIFFS partition
spiffs_create_partition_image(voice "${CMAKE_SOURCE_DIR}/voice" FLASH_IN_PROJECT)
