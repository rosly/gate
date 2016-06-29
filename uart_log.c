
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <boards.h> /* include nrf_gpio.h and pca*.h based on BOARD_* 
                       define from makefile */
#include <nrf_uart.h>

inline static void log_write_string(const char* msg)
{
    while (*msg)
    {
        nrf_uart_txd_set(NRF_UART0, *msg++);
        /* spin while pending TX */
        while (!nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_TXDRDY));
        nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_TXDRDY);
    }
}

void log_init()
{
    nrf_uart_disable(NRF_UART0);

    // Configure RX/TX RTS/CTS pins
    nrf_gpio_cfg_output(TX_PIN_NUMBER);
    nrf_gpio_cfg_input(RX_PIN_NUMBER, NRF_GPIO_PIN_NOPULL);
    nrf_uart_txrx_pins_set(NRF_UART0, TX_PIN_NUMBER, RX_PIN_NUMBER);
    nrf_uart_hwfc_pins_set(NRF_UART0, 0xFFFFFFFF, 0xFFFFFFFF);

    // Configure speed
    nrf_uart_baudrate_set(NRF_UART0, NRF_UART_BAUDRATE_115200);

    // Disable parity and HWFlowControl
    nrf_uart_configure(NRF_UART0, 
                       NRF_UART_PARITY_EXCLUDED,
                       NRF_UART_HWFC_DISABLED);

    // Re-enable the UART
    nrf_uart_enable(NRF_UART0);
    nrf_uart_int_enable(NRF_UART0, 0);
    nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTRX);
    nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTTX);
}

void log_printf(const char * format_msg, ...)
{
    char buffer[256];

    va_list p_args;
    va_start(p_args, format_msg);
    vsnprintf(buffer, sizeof(buffer), format_msg, p_args);
    va_end(p_args);

    log_write_string(buffer);
    log_write_string("\r\n");
}

void log_hex(const char* msg, const uint8_t * p_data, const uint32_t len)
{
    size_t i = 0;
    char buffer[4];
    
    log_write_string(msg);
    while (i < len)
    {
        snprintf(buffer, sizeof(buffer), " %02X", p_data[i]);
        log_write_string(buffer);
		  if (!(++i % 16))
				log_write_string("\r\n");
    }

    log_write_string("\r\n");
}


