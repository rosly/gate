#include <boards.h> /* include nrf_gpio.h and pca*.h based on BOARD_* define from makefile */
#include <nrf_uart.h>

void log_uart_init(void)
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

void log_uart_write_string(const char* msg, size_t len)
{
	size_t i = 0;

	while (i < len)
	{
		nrf_uart_txd_set(NRF_UART0, msg[i++]);
		/* spin while pending TX */
		while (!nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_TXDRDY));
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_TXDRDY);
	}
}

