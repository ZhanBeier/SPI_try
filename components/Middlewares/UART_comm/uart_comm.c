#include "uart_comm.h"
#include "driver/uart.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define OUT_UART_PORT UART_NUM_0
#define OUT_UART_TX 43
#define OUT_UART_RX 44

void uart_comm_init(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(OUT_UART_PORT, 1024, 256, 0, NULL, 0);
    uart_param_config(OUT_UART_PORT, &cfg);
    uart_set_pin(OUT_UART_PORT, OUT_UART_TX, OUT_UART_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void uart_comm_print(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0)
        uart_write_bytes(OUT_UART_PORT, buf, len);
}

int uart_comm_read_byte(uint8_t *c, int timeout_ms)
{
    return uart_read_bytes(OUT_UART_PORT, c, 1, pdMS_TO_TICKS(timeout_ms));
}