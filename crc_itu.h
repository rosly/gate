#ifndef __CRC_ITU_H__
#define __CRC_ITU_H__

#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

uint16_t crc_itu(uint16_t crc, const uint8_t *buffer, size_t len);

#endif
