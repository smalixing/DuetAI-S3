idf_component_register(SRCS "main.c" "lvgl_port.c" "wake_word_task.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bsp lvgl os_hal spi_flash esp-sr)
