#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <log_uart.h>
#include <log_disc.h>

#include <log_debug.h>

void log_debug_init(void)
{
	log_uart_init();
	log_disc_init();
}

static void log_debug_write_string(bool perm, const char* buffer, size_t len)
{
	/* in case permanent debug, store it also in disc */
	if (perm)
		log_dics_write_string(buffer, len);
	log_uart_write_string(buffer, len);
}

static void log_debug_printf_internal(bool perm, const char * format_msg, ...)
{
	int ret;
	char buffer[256];

	va_list p_args;
	va_start(p_args, format_msg);
	ret = vsnprintf(buffer, sizeof(buffer), format_msg, p_args);
	if (ret > 0) {
		/* write \r\n at the end of formated string in case there was no overflow */
		size_t off = ((ret + 2) < sizeof(buffer)) ? ret : (sizeof(buffer) - 2);
		buffer[off++] = '\r';
		buffer[off++] = '\n';
		log_debug_write_string(perm, buffer, off);
	}
	va_end(p_args);
}

/** Debug messages which cannot be stored on persistent media
 * Such calls can be used in the file system and disc block driver itself
 * Also can be used for high frequency debugs
 */
void log_debug_volatile_printf(const char * format_msg, ...)
{
	va_list p_args;

	va_start(p_args, format_msg);
	log_debug_printf_internal(false, format_msg, p_args);
	va_end(p_args);
}

void log_debug_printf(const char * format_msg, ...)
{
	va_list p_args;

	va_start(p_args, format_msg);
	log_debug_printf_internal(true, format_msg, p_args);
	va_end(p_args);
}

void log_debug_hex(const char* msg, const uint8_t * p_data, size_t cnt)
{
	size_t i, len, off;
	int ret;
	char buffer[16 * 3 + 2]; /* 16 hex + end of line */

	len = strlen(msg);
	off = len < (sizeof(buffer) - 2) ? len : (sizeof(buffer) - 2);
	memcpy(buffer, msg, off);
	buffer[off++] = '\r';
	buffer[off++] = '\n';
	log_debug_write_string(true, buffer, off);

	i = 0; off = 0;
	while (i < cnt)
	{
		len = sizeof(buffer) - off - 2; /* 2 chars reserved for end of line */
		ret = snprintf(&buffer[off], len, "%02X ", p_data[i]);
		off += (ret < len) ? ret : len;
		if (!(++i % 16)) {
			buffer[off++] = '\r';
			buffer[off++] = '\n';
			log_debug_write_string(true, buffer, off);
			off = 0; /* start from begining */
		}
	}

	/* if cnt was not multiply of 16 we will have something left to print */
	if (off)
			log_debug_write_string(true, buffer, off);
}

