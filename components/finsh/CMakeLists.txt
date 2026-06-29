set(sources "shell.c"
            "msh.c"
            "msh_parse.c"
            "finsh_hal.c")

idf_component_register(SRCS ${sources}
                    INCLUDE_DIRS .
                    PRIV_REQUIRES driver
                    LDFRAGMENTS "finsh_link.lf")

