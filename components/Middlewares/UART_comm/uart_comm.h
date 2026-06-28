#ifndef UART_COMM_H
#define UART_COMM_H

#include <stddef.h>
#include <stdint.h>

void uart_comm_init(void);
void uart_comm_print(const char *fmt, ...);
int  uart_comm_read_byte(uint8_t *c, int timeout_ms);

#endif