#ifndef _LOG_UART_H_
#define _LOG_UART_H_

#include <stddef.h>

void log_uart_init(void);
void log_uart_write_string(const char* msg, size_t len);

#endif

