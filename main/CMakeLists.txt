idf_component_register(SRCS "main.c" "cmd_line.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_linenoise esp_driver_usb_serial_jtag BSP)
