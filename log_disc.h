#ifndef _LOG_DISK_H_
#define _LOG_DISK_H_

#include <stddef.h>

void log_disc_init(void);
void log_disc_finalize(void);
void log_dics_write_string(const char* msg, size_t len);

#endif
