#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include "nrf_gpio.h" /* for nrf_gpio_* functions */
#include "pca10028.h" /* for EVB boards pin defs */

void board_setup(void)
{
   nrf_gpio_cfg_output(LEDS_MASK); /* make all led pins as output */
   nrf_gpio_pin_clear(LEDS_MASK);
   //app_trace_init();
}

int main(void)
{
    board_setup();

    //app_trace_log("Main loop\r\n");
    nrf_gpio_pin_clear(BSP_LED_0_MASK);
}

