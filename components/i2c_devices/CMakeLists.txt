
idf_component_register(
    SRC_DIRS
        "icm42670"
        "touch_panel"
        "ALS1206AC"
    INCLUDE_DIRS
        "icm42670/include"
        "touch_panel/include"
        "ALS1206AC"
    REQUIRES
        bsp
        esp_rom)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-incompatible-pointer-types)
