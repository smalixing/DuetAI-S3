idf_component_register(SRCS "main.c" "lvgl_port.c" "lvgl_mem_psram.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bsp lvgl os_hal spi_flash esp_timer
                                esp_app_format esp_psram esp_hw_support
                                i2c_devices
                       # lvgl_mem_psram.c provides the LVGL custom allocator
                       # hooks, which are referenced only from liblvgl.a —
                       # whole-archive keeps the object file in the link.
                       WHOLE_ARCHIVE)

