idf_component_register(SRCS "main.c" "lvgl_port.c"
                       REQUIRES bsp lvgl os_hal spi_flash)
