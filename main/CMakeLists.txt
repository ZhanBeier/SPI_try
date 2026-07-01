idf_component_register(SRCS "cmd_line.c" "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver BSP UART_comm NTC_calc LTC6811)

set(EXTRA_COMPONENT_DIRS components/Middlewares)
