if(${IDF_VERSION_MAJOR} EQUAL 5)
    idf_component_register(SRCS "button_adc.c" "button_gpio.c" "iot_button.c"
                            INCLUDE_DIRS include
                            PRIV_REQUIRES esp_adc driver esp_timer)
else()
    idf_component_register(SRCS "button_adc.c" "button_gpio.c" "iot_button.c"
                            INCLUDE_DIRS include
                            PRIV_REQUIRES esp_adc_cal driver esp_timer)
endif()


target_compile_options(${COMPONENT_LIB} PRIVATE    
-Wno-unused-variable
-Wno-unused-function
-Wno-format
)
