#ifndef _LOG_UART_H_
#define _LOG_UART_H_

void log_uart_init();
void log_uart_printf(const char * format_msg, ...);
void log_uart_hex(const char* msg, const uint8_t * p_data, size_t len);

#endif

