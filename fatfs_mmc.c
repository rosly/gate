/*------------------------------------------------------------------------*/
/* STM32F100: MMCv3/SDv1/SDv2 (SPI mode) control module                   */
/*------------------------------------------------------------------------*/
/*
/  Copyright (C) 2014, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/

#define SPI_MASTER_1_ENABLE
#include "nrf_drv_config.h"
#include "nrf_spi.h"
#include "nrf_gpio.h"
#include <sdk_errors.h> /* for ret_code_t and error defines */

#include "uart_log.h"

#include "fatfs_diskio.h"

#if 0
#define SPI_CH	1	/* SPI channel to use = 1: SPI1, 11: SPI1/remap, 2: SPI2 */

#define FCLK_SLOW() { SPIx_CR1 = (SPIx_CR1 & ~0x38) | 0x28; }	/* Set SCLK = PCLK / 64 */
#define FCLK_FAST() { SPIx_CR1 = (SPIx_CR1 & ~0x38) | 0x00; }	/* Set SCLK = PCLK / 2 */

#if SPI_CH == 1	/* PA4:MMC_CS, PA5:MMC_SCLK, PA6:MMC_DO, PA7:MMC_DI, PC4:MMC_CD */
#define CS_HIGH()	GPIOA_BSRR = _BV(4)
#define CS_LOW()	GPIOA_BSRR = _BV(4+16)
#define	MMC_CD		!(GPIOC_IDR & _BV(4))	/* Card detect (yes:true, no:false, default:true) */
#define	MMC_WP		0 /* Write protected (yes:true, no:false, default:false) */
#define SPIx_CR1	SPI1_CR1
#define SPIx_SR		SPI1_SR
#define SPIx_DR		SPI1_DR
#define	SPIxENABLE() {\
	__enable_peripheral(SPI1EN);\
	__enable_peripheral(IOPAEN);\
	__enable_peripheral(IOPCEN);\
	__gpio_conf_bit(GPIOA, 4, OUT_PP);						/* PA4: MMC_CS */\
	__gpio_conf_bit(GPIOA, 5, ALT_PP);						/* PA5: MMC_SCLK */\
	GPIOA_BSRR = _BV(6); __gpio_conf_bit(GPIOA, 6, IN_PUL); /* PA6: MMC_DO with pull-up */\
	__gpio_conf_bit(GPIOA, 7, ALT_PP);						/* PA7: MMC_DI */\
	GPIOC_BSRR = _BV(4); __gpio_conf_bit(GPIOC, 4, IN_PUL);	/* PC4: MMC_CD with pull-up */\
	SPIx_CR1 = _BV(9)|_BV(8)|_BV(6)|_BV(2);					/* Enable SPI1 */\
}

#elif SPI_CH == 11	/* PA15:MMC_CS, PB3:MMC_SCLK, PB4:MMC_DO, PB5:MMC_DI, PB6:MMC_CD */
#define CS_HIGH()	GPIOA_BSRR = _BV(15)
#define CS_LOW()	GPIOA_BSRR = _BV(15+16)
#define	MMC_CD		!(GPIOB_IDR & _BV(6))	/* Card detect (yes:true, no:false, default:true) */
#define	MMC_WP		0 /* Write protected (yes:true, no:false, default:false) */
#define SPIx_CR1	SPI1_CR1
#define SPIx_SR		SPI1_SR
#define SPIx_DR		SPI1_DR
#define	SPIxENABLE() {\
	AFIO_MAPR |= _BV(1);
	__enable_peripheral(SPI1EN);\
	__enable_peripheral(IOPAEN);\
	__enable_peripheral(IOPBEN);\
	__gpio_conf_bit(GPIOA, 15, OUT_PP); 						/* PA15: MMC_CS */\
	__gpio_conf_bit(GPIOB, 3, ALT_PP);							/* PB3: MMC_SCLK */\
	GPIOB_BSRR = _BV(4); __gpio_conf_bit(GPIOB, 4, IN_PUL);		/* PB4: MMC_DO with pull-up */\
	__gpio_conf_bit(GPIOB, 5, ALT_PP);							/* PB5: MMC_DI */\
	GPIOB_BSRR = _BV(6); __gpio_conf_bit(GPIOB, 6, IN_PUL);		/* PB6: MMC_CD with pull-up */\
	SPIx_CR1 = _BV(9)|_BV(8)|_BV(6)|_BV(2);						/* Enable SPI1 */\
}

#elif SPI_CH == 2	/* PB12:MMC_CS, PB13:MMC_SCLK, PB14:MMC_DO, PB15:MMC_DI, PD8:MMC_CD */
#define CS_HIGH()	GPIOB_BSRR = _BV(12)
#define CS_LOW()	GPIOB_BSRR = _BV(12+16)
#define	MMC_CD		!(GPIOD_IDR & _BV(8))	/* Card detect (yes:true, no:false, default:true) */
#define	MMC_WP		0 /* Write protected (yes:true, no:false, default:false) */
#define SPIx_CR1	SPI2_CR1
#define SPIx_SR		SPI2_SR
#define SPIx_DR		SPI2_DR
#define	SPIxENABLE() {\
	__enable_peripheral(SPI2EN);\
	__enable_peripheral(IOPBEN);\
	__enable_peripheral(IOPDEN);\
	__gpio_conf_bit(GPIOB, 12, OUT_PP);							/* PB12: MMC_CS */\
	__gpio_conf_bit(GPIOB, 13, ALT_PP);							/* PB13: MMC_SCLK */\
	GPIOB_BSRR = _BV(14); __gpio_conf_bit(GPIOB, 14, IN_PUL); 	/* PB14: MMC_DO with pull-up */\
	__gpio_conf_bit(GPIOB, 15, ALT_PP);							/* PB15: MMC_DI */\
	GPIOD_BSRR = _BV(8); __gpio_conf_bit(GPIOD, 8, IN_PUL); 	/* PD8: MMC_CD with pull-up */\
	SPIx_CR1 = _BV(9)|_BV(8)|_BV(6)|_BV(2);						/* Enable SPI1 */\
}

#endif
#endif

/* SPI IO defines */
#define MMC_SPI NRF_SPI1
#define SPI_CONFIG_SCK_PIN SPI1_CONFIG_SCK_PIN
#define SPI_CONFIG_MISO_PIN SPI1_CONFIG_MISO_PIN
#define SPI_CONFIG_MOSI_PIN SPI1_CONFIG_MOSI_PIN
#define SPI_CONFIG_CS_PIN 5

#define CS_HIGH() nrf_gpio_pin_set(SPI_CONFIG_CS_PIN)
#define CS_LOW() nrf_gpio_pin_clear(SPI_CONFIG_CS_PIN)

/* MMC/SD command */
#define CMD0	(0)		/* GO_IDLE_STATE */
#define CMD1	(1)		/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)		/* SEND_IF_COND */
#define CMD9	(9)		/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */

struct mmc_drv {
	volatile DSTATUS mmc_status;
	volatile uint8_t card_type;
	volatile size_t timer1;
	volatile size_t timer2;
};

volatile struct mmc_drv mmc_drv = {
	.mmc_status = STA_NOINIT,	/* Physical drive status */
	.card_type = CT_UNKNOWN,	/* Card type flags */
	.timer1 = 0,			/* 1kHz decrement timer stopped at zero (disk_timerproc()) */
	.timer2 = 0,
};

/*-----------------------------------------------------------------------*/
/* CRC calculation helper functions                                      */
/*-----------------------------------------------------------------------*/

/** CRC table for the CRC ITU-T V.41 0x0x1021 (x^16 + x^12 + x^15 + 1) */
static const uint16_t crc_itu_t_table[256] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

/**
 * Implements the standard CRC ITU-T V.41:
 *
 * Width 16
 * Poly  0x0x1021 (x^16 + x^12 + x^15 + 1)
 * Init  0
 *
 * @crc:     previous CRC value
 * @data:    data
 * @return:  Returns the updated CRC value
 */
static inline uint16_t crc_itu_t_byte(uint16_t crc, const uint8_t data)
{
	return (crc << 8) ^ crc_itu_t_table[((crc >> 8) ^ data) & 0xff];
}

/**
 * crc_itu_t - Compute the CRC-ITU-T for the data buffer
 *
 * @crc:     previous CRC value
 * @buffer:  data pointer
 * @len:     number of bytes in the buffer
 * @return:  Returns the updated CRC value
 */
uint16_t crc_itu_t(uint16_t crc, const uint8_t *buffer, size_t len)
{
	while (len--)
		crc = crc_itu_t_byte(crc, *buffer++);
	return crc;
}

/*-----------------------------------------------------------------------*/
/* SPI controls (Platform dependent)                                     */
/*-----------------------------------------------------------------------*/

/* Initialize MMC interface */
static int spi_init(void)
{
	//A Slave select must be set as high before setting it as output,
	//because during connect it to the pin it causes glitches.
	nrf_gpio_pin_set(SPI_CONFIG_CS_PIN);
	nrf_gpio_cfg_output(SPI_CONFIG_CS_PIN);
	nrf_gpio_pin_set(SPI_CONFIG_CS_PIN);
	nrf_gpio_cfg_output(SPI_CONFIG_SCK_PIN);
	nrf_gpio_cfg_output(SPI_CONFIG_MOSI_PIN);
	nrf_gpio_cfg_input(SPI_CONFIG_MISO_PIN, NRF_GPIO_PIN_NOPULL);

	nrf_spi_pins_set(MMC_SPI, SPI_CONFIG_SCK_PIN,
			 SPI_CONFIG_MOSI_PIN, SPI_CONFIG_MISO_PIN);
	nrf_spi_frequency_set(MMC_SPI, NRF_SPI_FREQ_250K);
	nrf_spi_configure(MMC_SPI, NRF_SPI_MODE_0, NRF_SPI_BIT_ORDER_MSB_FIRST);
	nrf_spi_enable(MMC_SPI);

	nrf_spi_event_clear(MMC_SPI, NRF_SPI_EVENT_READY);

	return 0;
}

static int spi_init_fast(void)
{
	nrf_spi_disable(MMC_SPI);
	/* 3 CPU cycle delay for 1M
	 * 1 CPu cycle delay for 2 and 4M
	 * 0 CPU cycle delay for 8M */
	nrf_spi_frequency_set(MMC_SPI, NRF_SPI_FREQ_8M);
	nrf_spi_enable(MMC_SPI);

	return 0;
}

/**
 * Function check the communication delay for single byte
 *
 * Function is for debug purposes only
 */
static void spi_check_delay(void)
{
	const size_t cnt = 10;
	size_t i;
	size_t del = 0;
	uint8_t ret;

	/* Send one byte at a time and sum the delay */
	for (i = cnt; i > 0; i--) {
		nrf_spi_txd_set(MMC_SPI, 0xff);
		while (!nrf_spi_event_check(MMC_SPI, NRF_SPI_EVENT_READY))
			del++;
		nrf_spi_event_clear(MMC_SPI, NRF_SPI_EVENT_READY);
		ret = nrf_spi_rxd_get(MMC_SPI);
		if (ret != 0xff)
			log_printf("Received invalid dummy data 0x%02x", ret);
	}

	log_printf("SPI communication delay %u CPU cycles", del / cnt);
}

/* Exchange a byte */
static uint8_t spi_xchg(uint8_t dat)
{
	size_t i = 0;
	uint8_t ret;

	/* transmiter will start SPI clock after write operation to TXD register
	 * TXD register is double buffered (same RXD) */
	nrf_spi_txd_set(MMC_SPI, dat);
	while (!nrf_spi_event_check(MMC_SPI, NRF_SPI_EVENT_READY))
		i++;
	nrf_spi_event_clear(MMC_SPI, NRF_SPI_EVENT_READY);
	ret = nrf_spi_rxd_get(MMC_SPI);

	return ret;
}

/* Receive multiple byte */
static int spi_rcvr_multi(uint8_t *buff, size_t cnt)
{
	size_t rx = 0;

	if (cnt == 0)
		return 0;

	/* transmiter will start SPI clock after write operation to TXD register
	 * TXD register is double buffered (same RXD) so we put two bytes there in
	 * serie. Start transmiting first byte */
	nrf_spi_txd_set(MMC_SPI, 0xff);

	/* next, xmit and receive required amout of bytes, beside the last one */
	while (cnt > (rx + 1)) {
		nrf_spi_txd_set(MMC_SPI, 0xff);

		/* wait until xmit and receive is finished */
		while (!nrf_spi_event_check(MMC_SPI, NRF_SPI_EVENT_READY));
		nrf_spi_event_clear(MMC_SPI, NRF_SPI_EVENT_READY);

		buff[rx++] = nrf_spi_rxd_get(MMC_SPI);
	}

	/* finally, wait and receive te final byte which was cheduled by double
	 * bufered operation. In case cnt == 1 we will not execute the upper loop */
	while (!nrf_spi_event_check(MMC_SPI, NRF_SPI_EVENT_READY));
	nrf_spi_event_clear(MMC_SPI, NRF_SPI_EVENT_READY);

	buff[rx++] = nrf_spi_rxd_get(MMC_SPI);

	return 0;
}

#if 0
#if _USE_WRITE
/* Send multiple byte */
static
void spi_xmit_multi (
	const uint8_t *buff,	/* Pointer to the data */
	size_t btx			/* Number of bytes to send (even number) */
)
{
	WORD d;


	SPIx_CR1 &= ~_BV(6);
	SPIx_CR1 |= (_BV(6) | _BV(11));		/* Set SPI to 16-bit mode */

	d = buff[0] << 8 | buff[1];
	SPIx_DR = d;
	buff += 2;
	btx -= 2;
	do {					/* Receive the data block into buffer */
		d = buff[0] << 8 | buff[1];
		while (SPIx_SR & _BV(7)) ;
		SPIx_DR;
		SPIx_DR = d;
		buff += 2;
	} while (btx -= 2);
	while (SPIx_SR & _BV(7)) ;
	SPIx_DR;

	SPIx_CR1 &= ~(_BV(6) | _BV(11));	/* Set SPI to 8-bit mode */
	SPIx_CR1 |= _BV(6);
}
#endif

#endif

/*-----------------------------------------------------------------------*/
/* MMC Card specific functions                                           */
/*-----------------------------------------------------------------------*/

/** Wait for card
 *
 * @param wt Timeout [ms]
 * @return 1:Ready, 0:Timeout
 */
static int card_wait_ready(size_t wt)
{
	uint8_t d;

	mmc_drv.timer2 = wt;
	do {
		d = spi_xchg(0xFF);
		/* This loop takes a time. Insert rot_rdq() here for multitask environment. */
	} while (d != 0xFF && mmc_drv.timer2);	/* Wait for card goes ready or timeout */

	return (d == 0xFF) ? 1 : 0;
}

/** Deselect card and release SPI
 */
static void card_deselect(void)
{
	/* Set CS# high */
	CS_HIGH();
	/* Dummy clock (force DO hi-z for multiple slave SPI) */
	spi_xchg(0xFF);
}

/** Enable chip select for the card and verify its activation
 *
 * @return 1 - OK, 0 - Timeout
 */
static int card_select(void)	/* 1:OK, 0:Timeout */
{
	CS_LOW();	/* Set CS# low */
	//spi_xchg(0xFF);	/* Dummy clock (force DO enabled) */

	/* Wait for card ready */
	if (card_wait_ready(500))
		return 1; /* OK */

	card_deselect();
	return 0; /* Timeout */
}

/**
 * Send command to MMC card
 *
 * @param cmd - the command to send
 * @param arg - argument
 * @return R1 resp (bit7==1:Failed to send)
 */
static uint8_t card_send_cmd(uint8_t cmd, uint32_t arg)
{
	uint8_t n, res;

	/* Send a CMD55 prior to ACMD<n> */
	if (cmd & 0x80) {
		cmd &= 0x7F;
		res = card_send_cmd(CMD55, 0);
		if (res > 1)
			return res;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		card_deselect();
		if (!card_select())
			return 0xFF;
	}

	/* Send command packet */
	spi_xchg(0x40 | cmd);			/* Start + command index */
	spi_xchg((uint8_t)(arg >> 24));		/* Argument[31..24] */
	spi_xchg((uint8_t)(arg >> 16));		/* Argument[23..16] */
	spi_xchg((uint8_t)(arg >> 8));		/* Argument[15..8] */
	spi_xchg((uint8_t)arg);			/* Argument[7..0] */
	n = 0x01;				/* Dummy CRC + Stop */
	if (cmd == CMD0)
		n = 0x95;			/* Valid CRC for CMD0(0) */
	if (cmd == CMD8)
		n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
	spi_xchg(n);

	/* Receive command resp */
	if (cmd == CMD12)
		spi_xchg(0xFF);			/* Diacard following one byte when CMD12 */
	n = 10;					/* Wait for response (10 bytes max) */
	do {
		res = spi_xchg(0xFF);
	} while ((res & 0x80) && --n);

	return res;				/* Return received response */
}

/** Receive the data block from card
 *
 * @param buff - Data buffer
 * @buff_size - Size of data buffer
 * @return 1 - OK, 0 - Error
 */
static int card_rcv_block(uint8_t *buff, size_t buff_size)
{
	uint16_t crc_rcv;
	uint16_t crc_cal;
	uint8_t token;

	/* Wait for DataStart token in timeout of 200ms */
	mmc_drv.timer1 = 200;
	do {
		token = spi_xchg(0xFF);
		/* This loop will take a time. Insert rot_rdq() here for multitask envilonment. */
	} while ((token == 0xFF) && mmc_drv.timer1);

	/* Function fails if invalid DataStart token or timeout */
	if(token != 0xFE)
		return 0; /* Error */

	/* Store data to the buffer */
	spi_rcvr_multi(buff, buff_size);

	/* Receive CRC */
	crc_rcv = spi_xchg(0xFF);
	crc_rcv = (crc_rcv << 8) | spi_xchg(0xFF);

	/* Calculate CRC */
	crc_cal = crc_itu_t(0x0000, buff, buff_size);

	/* compare CRC's */
	if (crc_cal != crc_rcv) {
		log_printf("CRC error!");
		return 0; /* Error */
	}

	return 1; /* Success */
}

#if 0
#if _USE_WRITE
static
int xmit_datablock (	/* 1:OK, 0:Failed */
	const uint8_t *buff,	/* Ponter to 512 byte data to be sent */
	uint8_t token			/* Token */
)
{
	uint8_t resp;


	if (!card_wait_ready(500)) return 0;		/* Wait for card ready */

	spi_xchg(token);					/* Send token */
	if (token != 0xFD) {				/* Send data if token is other than StopTran */
		spi_xmit_multi(buff, 512);		/* Data */
		spi_xchg(0xFF); spi_xchg(0xFF);	/* Dummy CRC */

		resp = spi_xchg(0xFF);				/* Receive data resp */
		if ((resp & 0x1F) != 0x05)		/* Function fails if the data packet was not accepted */
			return 0;
	}
	return 1;
}
#endif
#endif

/*--------------------------------------------------------------------------
 * Public Functions
 *--------------------------------------------------------------------------*/

/**
 * Initialize the MMC disk layer
 *
 * @param drv - Physical drive number (0)
 * @return Disk status
 */
DSTATUS disk_initialize(uint8_t drv)
{
	uint8_t n, cmd, card_type, ocr[4];

	/* Supports only drive 0 */
	if (drv > 0)
		return STA_NOINIT;

	/* Initialize SPI */
	if (spi_init())
		return STA_NOINIT;

	/* Is card existing in the socket? */
	if (mmc_drv.mmc_status & STA_NODISK)
		return mmc_drv.mmc_status;

	/* Send 80 dummy clocks */
	for (n = 10; n; n--)
		spi_xchg(0xFF);

	card_type = CT_UNKNOWN;
	/* Put the card SPI/Idle state */
	if (card_send_cmd(CMD0, 0) == 0x01) {

		/* Initialization timeout = 1 sec */
		mmc_drv.timer1 = 1000;

		/* SDv2? */
		if (card_send_cmd(CMD8, 0x1AA) == 0x01) {
			/* Get 32 bit return value of R7 resp */
			for (n = 0; n < 4; n++)
				ocr[n] = spi_xchg(0xFF);

			/* Is the card supports vcc of 2.7-3.6V? */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
				/* Wait for end of initialization with ACMD41(HCS) */
				while (mmc_drv.timer1 && card_send_cmd(ACMD41, 1UL << 30));

				/* Check CCS bit in the OCR */
				if (mmc_drv.timer1 && card_send_cmd(CMD58, 0) == 0) {
					for (n = 0; n < 4; n++)
						ocr[n] = spi_xchg(0xFF);
					/* Card id SDv2 */
					card_type = (ocr[0] & 0x40) ?
						CT_SD2 | CT_BLOCK : CT_SD2;
				}
			}
		} else { /* Not SDv2 card */
			/* SDv1 or MMC? */
			if (card_send_cmd(ACMD41, 0) <= 1) {
				/* SDv1 (ACMD41(0)) */
				card_type = CT_SD1;
				cmd = ACMD41;
			} else {
				/* MMCv3 (CMD1(0)) */
				card_type = CT_MMC;
				cmd = CMD1;
			}
			/* Wait for end of initialization */
			while (mmc_drv.timer1 && card_send_cmd(cmd, 0));

			/* Set block length: 512 */
			if (!mmc_drv.timer1 || card_send_cmd(CMD16, 512) != 0)
				card_type = CT_UNKNOWN;
		}
	}
	mmc_drv.card_type = card_type;
	card_deselect();

	if (card_type != CT_UNKNOWN) {
		/* Set fast clock */
		if (spi_init_fast())
			return STA_NOINIT;

		/* Verify the speed settings */
		spi_check_delay();

		/* Clear STA_NOINIT flag */
		mmc_drv.mmc_status &= ~STA_NOINIT;
	} else {
		/* Failed */
		mmc_drv.mmc_status = STA_NOINIT;
	}

	return mmc_drv.mmc_status;
}

#if 0

DSTATUS disk_status (
	uint8_t drv		/* Physical drive number (0) */
)
{
	if (drv) return STA_NOINIT;		/* Supports only drive 0 */

	return mmc_drv.mmc_status;	/* Return disk status */
}
#endif

/** Read sector(s)
 *
 * @param drv - Physical drive number (0)
 * @param buff - Pointer to the data buffer to store read data
 * @param sector - Start sector number (LBA)
 * @param count Number of sectors to read (1..128)
 */
DRESULT disk_read(uint8_t drv, uint8_t *buff, uint32_t sector, size_t count)
{
	/* Check parameters (only one disc, > 1 sector) */
	if ((drv > 0) || !count)
		return RES_PARERR;

	/* Check if drive is ready */
	if (mmc_drv.mmc_status & STA_NOINIT)
		return RES_NOTRDY;

	/* LBA ot BA conversion (byte addressing cards) */
	if (!(mmc_drv.card_type & CT_BLOCK))
		sector *= 512;

	if (count == 1) {
		/* Single sector read */
		if ((card_send_cmd(CMD17, sector) == 0) &&
			 card_rcv_block(buff, 512))
				count = 0; /* OK */
	} else {
		/* Multiple sector read */
		if (card_send_cmd(CMD18, sector) == 0) {
			do {
				if (!card_rcv_block(buff, 512))
					break;
				buff += 512;
			} while (--count);
			/* Stop multiple block transmision */
			card_send_cmd(CMD12, 0);

			/* TODO the last command may interfere with tuffing
			 * bytes from last COM18. Do card_send_cmd() is prepared
			 * for such unexpected bytes ? */
		}
	}
	card_deselect();

	return count ? RES_ERROR : RES_OK;	/* Return result */
}

#if 0
/** Write sector(s)
 */
#if _USE_WRITE
DRESULT disk_write (
	uint8_t drv,			/* Physical drive number (0) */
	const uint8_t *buff,	/* Ponter to the data to write */
	uint32_t sector,		/* Start sector number (LBA) */
	size_t count			/* Number of sectors to write (1..128) */
)
{
	if (drv || !count) return RES_PARERR;		/* Check parameter */
	if (mmc_drv.mmc_status & STA_NOINIT) return RES_NOTRDY;	/* Check drive status */
	if (mmc_drv.mmc_status & STA_PROTECT) return RES_WRPRT;	/* Check write protect */

	if (!(mmc_drv.card_type & CT_BLOCK)) sector *= 512;	/* LBA ==> BA conversion (byte addressing cards) */

	if (count == 1) {	/* Single sector write */
		if ((card_send_cmd(CMD24, sector) == 0)	/* WRITE_BLOCK */
			&& xmit_datablock(buff, 0xFE))
			count = 0;
	}
	else {				/* Multiple sector write */
		if (mmc_drv.card_type & CT_SDC) card_send_cmd(ACMD23, count);	/* Predefine number of sectors */
		if (card_send_cmd(CMD25, sector) == 0) {	/* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD))	/* STOP_TRAN token */
				count = 1;
		}
	}
	card_deselect();

	return count ? RES_ERROR : RES_OK;	/* Return result */
}
#endif

/** Miscellaneous drive controls other than data read/write
 */
#if _USE_IOCTL
DRESULT disk_ioctl (
	uint8_t drv,		/* Physical drive number (0) */
	uint8_t cmd,		/* Control command code */
	void *buff		/* Pointer to the conrtol data */
)
{
	DRESULT res;
	uint8_t n, csd[16];
	uint32_t *dp, st, ed, csize;


	if (drv) return RES_PARERR;					/* Check parameter */
	if (mmc_drv.mmc_status & STA_NOINIT) return RES_NOTRDY;	/* Check if drive is ready */

	res = RES_ERROR;

	switch (cmd) {
	case CTRL_SYNC :		/* Wait for end of internal write process of the drive */
		if (select()) res = RES_OK;
		break;

	case GET_SECTOR_COUNT :	/* Get drive capacity in unit of sector (uint32_t) */
		if ((card_send_cmd(CMD9, 0) == 0) && card_rcv_block(csd, 16)) {
			if ((csd[0] >> 6) == 1) {	/* SDC ver 2.00 */
				csize = csd[9] + ((WORD)csd[8] << 8) + ((uint32_t)(csd[7] & 63) << 16) + 1;
				*(uint32_t*)buff = csize << 10;
			} else {					/* SDC ver 1.XX or MMC ver 3 */
				n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
				csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
				*(uint32_t*)buff = csize << (n - 9);
			}
			res = RES_OK;
		}
		break;

	case GET_BLOCK_SIZE :	/* Get erase block size in unit of sector (uint32_t) */
		if (mmc_drv.card_type & CT_SD2) {	/* SDC ver 2.00 */
			if (card_send_cmd(ACMD13, 0) == 0) {	/* Read SD status */
				spi_xchg(0xFF);
				if (card_rcv_block(csd, 16)) {				/* Read partial block */
					for (n = 64 - 16; n; n--) spi_xchg(0xFF);	/* Purge trailing data */
					*(uint32_t*)buff = 16UL << (csd[10] >> 4);
					res = RES_OK;
				}
			}
		} else {					/* SDC ver 1.XX or MMC */
			if ((card_send_cmd(CMD9, 0) == 0) && card_rcv_block(csd, 16)) {	/* Read CSD */
				if (mmc_drv.card_type & CT_SD1) {	/* SDC ver 1.XX */
					*(uint32_t*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
				} else {					/* MMC */
					*(uint32_t*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
				}
				res = RES_OK;
			}
		}
		break;

	case CTRL_TRIM :	/* Erase a block of sectors (used when _USE_ERASE == 1) */
		if (!(mmc_drv.card_type & CT_SDC)) break;				/* Check if the card is SDC */
		if (disk_ioctl(drv, MMC_GET_CSD, csd)) break;	/* Get CSD */
		if (!(csd[0] >> 6) && !(csd[10] & 0x40)) break;	/* Check if sector erase can be applied to the card */
		dp = buff; st = dp[0]; ed = dp[1];				/* Load sector block */
		if (!(mmc_drv.card_type & CT_BLOCK)) {
			st *= 512; ed *= 512;
		}
		if (card_send_cmd(CMD32, st) == 0 && card_send_cmd(CMD33, ed) == 0 && card_send_cmd(CMD38, 0) == 0 && card_wait_ready(30000))	/* Erase sector block */
			res = RES_OK;	/* FatFs does not check result of this command */
		break;

	default:
		res = RES_PARERR;
	}

	card_deselect();

	return res;
}
#endif

/** Device timer function
 *
 * This function must be called from timer interrupt routine in period
 * of 1 ms to generate card control timing.
 */
void disk_timerproc (void)
{
	WORD n;
	uint8_t s;


	n = mmc_drv.timer1;						/* 1kHz decrement timer stopped at 0 */
	if (n) mmc_drv.timer1 = --n;
	n = mmc_drv.timer2;
	if (n) mmc_drv.timer2 = --n;

	s = mmc_drv.mmc_status;
	if (MMC_WP)		/* Write protected */
		s |= STA_PROTECT;
	else		/* Write enabled */
		s &= ~STA_PROTECT;
	if (MMC_CD)	/* Card is in socket */
		s &= ~STA_NODISK;
	else		/* Socket empty */
		s |= (STA_NODISK | STA_NOINIT);
	mmc_drv.mmc_status = s;
}
#endif

