#include <string.h>
#include "fatfs_ff.h"
#include "log_disc.h"

FATFS log_disc_fs;
FIL log_disc_file;

void log_disc_init(void)
{
	UINT bw;
	FSIZE_t fsize;

	do {
		if (f_mount(&log_disc_fs, "0:", 1)) {
			/* log_debug_volatile_printf("f_mount error"); */
			break;
		}

		if (FR_OK != f_open(&log_disc_file, "0:\\log.txt",
								  FA_WRITE | FA_OPEN_ALWAYS)) {
			/* log_debug_volatile_printf("Open file error"); */
			break;
		}

		/* move pointer to the end of file */
		fsize = f_size(&log_disc_file);
		if (FR_OK != f_lseek(&log_disc_file, fsize)) {
			/* log_debug_volatile_printf("Lseek error"); */
			break;
		}

		static const char start_log[] = "--- Debug started ---\r\n";
		if (FR_OK != f_write(&log_disc_file, start_log, strlen(start_log), &bw)) {
			 /* log_debug_volatile_printf("Write file error"); */
			 break;
		}
	} while (0);
}

void log_disc_finalize(void)
{
	if (FR_OK != f_close(&log_disc_file)) {
		 /* log_debug_volatile_printf("Close file error"); */
	}
}

void log_dics_write_string(const char* msg, size_t len)
{
	static size_t len_sum = 0;
	UINT bw;

	if (FR_OK != f_write(&log_disc_file, msg, len, &bw) || (bw != len)) {
		 /* log_debug_volatile_printf("Write file error len=%zu bw=%u", len, bw); */
		return;
	}

	len_sum += len;
	if (len_sum > 2048) {
		f_sync(&log_disc_file);
		len_sum = 0;
	}

}

