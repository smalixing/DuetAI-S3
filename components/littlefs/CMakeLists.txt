idf_component_register(
    SRCS 
        "src/lfs_util.c"
        "src/lfs.c"
        "port_esp32/esp_littlefs.c"
        "port_esp32/littlefs_api.c"
    INCLUDE_DIRS 
        "port_esp32"
    PRIV_INCLUDE_DIRS
        "src"
    REQUIRES 
        "spi_flash" "esp_partition" "vfs"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE -DCONFIG_LITTLEFS_MAX_PARTITIONS=2
                                            PRIVATE -DCONFIG_LITTLEFS_PAGE_SIZE=1024
                                            PRIVATE -DCONFIG_LITTLEFS_READ_SIZE=256
                                            PRIVATE -DCONFIG_LITTLEFS_WRITE_SIZE=256
                                            PRIVATE -DCONFIG_LITTLEFS_LOOKAHEAD_SIZE=256
                                            PRIVATE -DCONFIG_LITTLEFS_CACHE_SIZE=512
                                            PRIVATE -DCONFIG_LITTLEFS_BLOCK_CYCLES=512
                                        )
