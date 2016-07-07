#ifndef _LOG_DEBUG_H_
#define _LOG_DEBUG_H_

#include <stdarg.h>
#include "log_uart.h"

#ifndef NRF_LOG_USES_COLORS
    #define NRF_LOG_USES_COLORS 1
#endif

#if NRF_LOG_USES_COLORS == 1
    #define NRF_LOG_COLOR_DEFAULT  "\x1B[0m"
    #define NRF_LOG_COLOR_BLACK    "\x1B[1;30m"
    #define NRF_LOG_COLOR_RED      "\x1B[1;31m"
    #define NRF_LOG_COLOR_GREEN    "\x1B[1;32m"
    #define NRF_LOG_COLOR_YELLOW   "\x1B[1;33m"
    #define NRF_LOG_COLOR_BLUE     "\x1B[1;34m"
    #define NRF_LOG_COLOR_MAGENTA  "\x1B[1;35m"
    #define NRF_LOG_COLOR_CYAN     "\x1B[1;36m"
    #define NRF_LOG_COLOR_WHITE    "\x1B[1;37m"
#else
    #define NRF_LOG_COLOR_DEFAULT
    #define NRF_LOG_COLOR_BLACK
    #define NRF_LOG_COLOR_RED
    #define NRF_LOG_COLOR_GREEN
    #define NRF_LOG_COLOR_YELLOW
    #define NRF_LOG_COLOR_BLUE
    #define NRF_LOG_COLOR_MAGENTA
    #define NRF_LOG_COLOR_CYAN
    #define NRF_LOG_COLOR_WHITE
#endif

#define NRF_LOG_INIT()

#define NRF_LOG_PRINTF(...)             log_debug_printf( #__VA_ARGS__)
#define NRF_LOG_PRINTF_DEBUG(...)       NRF_LOG_PRINTF( #__VA_ARGS__)
#define NRF_LOG_PRINTF_ERROR(...)       NRF_LOG_PRINTF( #__VA_ARGS__)

#define NRF_LOG(...)                    log_debug_printf( #__VA_ARGS__)
#define NRF_LOG_DEBUG(...)              NRF_LOG_PRINTF( #__VA_ARGS__)
#define NRF_LOG_ERROR(...)              NRF_LOG_PRINTF( #__VA_ARGS__)

#define NRF_LOG_HEX(val)
#define NRF_LOG_HEX_DEBUG(val)
#define NRF_LOG_HEX_ERROR(val)

#define NRF_LOG_HEX_CHAR(val)
#define NRF_LOG_HEX_CHAR_DEBUG(val)
#define NRF_LOG_HEX_CHAR_ERROR(val)

#define NRF_LOG_HAS_INPUT()
#define NRF_LOG_READ_INPUT(p_char)

static inline void log_debug_init(void)
{
	log_uart_init();
}

static inline void log_debug_printf(const char * format_msg, ...)
{
    va_list p_args;
    va_start(p_args, format_msg);
    log_uart_printf(format_msg, p_args);
    va_end(p_args);
}

static inline void log_debug_hex(const char* msg, const uint8_t * p_data, 
											size_t len)
{
	log_uart_hex(msg, p_data, len);
}

/** Debug messages which cannot be stored on persistent media
 * Such calls can be used in the file system and disk block driver itself
 * Also can be used for high frequency debugs
 */
static inline void log_debug_volatile_printf(const char * format_msg, ...)
{
}

#endif
