idf_component_register(SRCS "main.c" "cmd_line.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_linenoise BSP)
