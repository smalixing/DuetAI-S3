set(sources "src/boards/bsp_board.c"
            "src/boards/esp32s3_dual_module.c"
            "src/boards/smart_plate.c"
            "src/peripherals/bsp_adc.c"
            "src/peripherals/bsp_btn.c"
            "src/peripherals/bsp_codec.c"
            "src/peripherals/bsp_gpio.c"
            "src/peripherals/bsp_i2c.c"
            "src/peripherals/bsp_i2s.c"
            "src/peripherals/bsp_lcd.c"
            "src/peripherals/bsp_ledc.c"
            "src/peripherals/bsp_rtm.c"
            "src/peripherals/bsp_uart.c"
            "src/codec/es7210/es7210.c"
            "src/codec/es7243e/es7243e.c"
            "src/codec/es8156/es8156.c"
            "src/codec/es8311/es8311.c"
            "src/codec/es8388/es8388.c"
            "src/lcd_panel/esp_lcd_gc9a01.c"
            "src/lcd_panel/lcd_panel_gc9307.c")

idf_component_register(SRCS ${sources}
                    INCLUDE_DIRS "include"
                                 "src/boards"
                                 "src/codec/es7210"
                                 "src/codec/es7243e"
                                 "src/codec/es8156"
                                 "src/codec/es8311"
                                 "src/codec/es8388"
                                 "src/codec/include"
                    REQUIRES driver esp_adc esp_lcd vfs iot_button i2c_bus)

# target_compile_options(${COMPONENT_LIB} PRIVATE
#     -Wno-unused-variable
#     -Wno-unused-function
#     -Wno-format
# )
