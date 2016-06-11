#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include <boards.h> /* include nrf_gpio.h and pca10028.h based on BOARD_* 
                       define from makefile */
#include "uart_log.h"

void board_setup(void)
{
   LEDS_CONFIGURE(LEDS_MASK);
   LEDS_OFF(LEDS_MASK);

   log_init();
}

int main(void)
{
    board_setup();

    LEDS_ON(BSP_LED_0_MASK);
    log_printf("Main loop\r\n");
}

