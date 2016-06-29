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

#define CS_HIGH() nrf_gpio_pin_set(5)
#define CS_LOW() nrf_gpio_pin_clear(5)
#define FCLK_SLOW()
#define FCLK_FAST()

/*--------------------------------------------------------------------------
   Module Private Functions
---------------------------------------------------------------------------*/

/* SPI used */
#define MMC_SPI NRF_SPI1
#define SPI_CONFIG_SCK_PIN SPI1_CONFIG_SCK_PIN
#define SPI_CONFIG_MISO_PIN SPI1_CONFIG_MISO_PIN
#define SPI_CONFIG_MOSI_PIN SPI1_CONFIG_MOSI_PIN
#define SPI_CONFIG_SS_PIN 5

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
/* SPI controls (Platform dependent)                                     */
/*-----------------------------------------------------------------------*/

/* Initialize MMC interface */
static int init_spi(void)
{
	//A Slave select must be set as high before setting it as output,
	//because during connect it to the pin it causes glitches.
	nrf_gpio_pin_set(SPI_CONFIG_SS_PIN);
	nrf_gpio_cfg_output(SPI_CONFIG_SS_PIN);
	nrf_gpio_pin_set(SPI_CONFIG_SS_PIN);
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

/* Exchange a byte */
static uint8_t xchg_spi(uint8_t dat)
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
	log_printf("xchg_spi() dat=0x%02x ret=0x%02x i=%u", dat, ret, i);

	return ret;
}

#if 0
/* Receive multiple byte */
static int rcvr_spi_multi(uint8_t *buff, size_t cnt)
{
	uint32_t err_code;

	err_code = spi_master_send_recv(MMC_SPI, NULL, 0, buff, cnt);
	if (err_code != NRF_SUCCESS) {
		log_printf("spi_master_send_recv error!");
		return -1;
	}

	return 0;
}

#if _USE_WRITE
/* Send multiple byte */
static
void xmit_spi_multi (
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
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

/** Wait for card
 *
 * @param wt Timeout [ms]
 * @return 1:Ready, 0:Timeout
 */
static int wait_ready(size_t wt)
{
	uint8_t d;

	mmc_drv.timer2 = wt;
	do {
		d = xchg_spi(0xFF);
		/* This loop takes a time. Insert rot_rdq() here for multitask environment. */
	} while (d != 0xFF && mmc_drv.timer2);	/* Wait for card goes ready or timeout */

	return (d == 0xFF) ? 1 : 0;
}

/*-----------------------------------------------------------------------*/
/* Deselect card and release SPI                                         */
/*-----------------------------------------------------------------------*/

static void deselect(void)
{
	CS_HIGH();	/* Set CS# high */
	xchg_spi(0xFF);	/* Dummy clock (force DO hi-z for multiple slave SPI) */
}

/*-----------------------------------------------------------------------*/
/* Select card and wait for ready                                        */
/*-----------------------------------------------------------------------*/

static
int select(void)	/* 1:OK, 0:Timeout */
{
	CS_LOW();	/* Set CS# low */
	//xchg_spi(0xFF);	/* Dummy clock (force DO enabled) */
	if (wait_ready(500))
		return 1; /* Wait for card ready */

	deselect();
	return 0;	/* Timeout */
}

#if 0
/*-----------------------------------------------------------------------*/
/* Receive a data packet from the MMC                                    */
/*-----------------------------------------------------------------------*/

static
int rcvr_datablock (	/* 1:OK, 0:Error */
	uint8_t *buff,			/* Data buffer */
	size_t btr			/* Data block length (byte) */
)
{
	uint8_t token;


	mmc_drv.timer1 = 200;
	do {							/* Wait for DataStart token in timeout of 200ms */
		token = xchg_spi(0xFF);
		/* This loop will take a time. Insert rot_rdq() here for multitask envilonment. */
	} while ((token == 0xFF) && mmc_drv.timer1);
	if(token != 0xFE) return 0;		/* Function fails if invalid DataStart token or timeout */

	rcvr_spi_multi(buff, btr);		/* Store trailing data to the buffer */
	xchg_spi(0xFF); xchg_spi(0xFF);			/* Discard CRC */

	return 1;						/* Function succeeded */
}



/*-----------------------------------------------------------------------*/
/* Send a data packet to the MMC                                         */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
static
int xmit_datablock (	/* 1:OK, 0:Failed */
	const uint8_t *buff,	/* Ponter to 512 byte data to be sent */
	uint8_t token			/* Token */
)
{
	uint8_t resp;


	if (!wait_ready(500)) return 0;		/* Wait for card ready */

	xchg_spi(token);					/* Send token */
	if (token != 0xFD) {				/* Send data if token is other than StopTran */
		xmit_spi_multi(buff, 512);		/* Data */
		xchg_spi(0xFF); xchg_spi(0xFF);	/* Dummy CRC */

		resp = xchg_spi(0xFF);				/* Receive data resp */
		if ((resp & 0x1F) != 0x05)		/* Function fails if the data packet was not accepted */
			return 0;
	}
	return 1;
}
#endif
#endif

/*-----------------------------------------------------------------------*/
/* Send a command packet to the MMC                                      */
/*-----------------------------------------------------------------------*/

/**
 * Send command to MMC card
 *
 * @param cmd - the command to send
 * @param arg - argument
 * @return R1 resp (bit7==1:Failed to send)
 */
static uint8_t send_cmd(uint8_t cmd, uint32_t arg)
{
	uint8_t n, res;

	/* Send a CMD55 prior to ACMD<n> */
	if (cmd & 0x80) {
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1)
			return res;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		deselect();
		if (!select())
			return 0xFF;
	}

	/* Send command packet */
	xchg_spi(0x40 | cmd);			/* Start + command index */
	xchg_spi((uint8_t)(arg >> 24));		/* Argument[31..24] */
	xchg_spi((uint8_t)(arg >> 16));		/* Argument[23..16] */
	xchg_spi((uint8_t)(arg >> 8));		/* Argument[15..8] */
	xchg_spi((uint8_t)arg);			/* Argument[7..0] */
	n = 0x01;				/* Dummy CRC + Stop */
	if (cmd == CMD0)
		n = 0x95;			/* Valid CRC for CMD0(0) */
	if (cmd == CMD8)
		n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
	xchg_spi(n);

	/* Receive command resp */
	if (cmd == CMD12)
		xchg_spi(0xFF);			/* Diacard following one byte when CMD12 */
	n = 10;					/* Wait for response (10 bytes max) */
	do {
		res = xchg_spi(0xFF);
	} while ((res & 0x80) && --n);

	return res;				/* Return received response */
}

/*--------------------------------------------------------------------------
   Public Functions
---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------*/
/* Initialize disk drive                                                 */
/*-----------------------------------------------------------------------*/

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
	if (init_spi())
		return STA_NOINIT;

	/* Is card existing in the socket? */
	if (mmc_drv.mmc_status & STA_NODISK)
		return mmc_drv.mmc_status;

	/* Send 80 dummy clocks */
	for (n = 10; n; n--)
		xchg_spi(0xFF);

	card_type = CT_UNKNOWN;
	/* Put the card SPI/Idle state */
	if (send_cmd(CMD0, 0) == 0x01) {

		/* Initialization timeout = 1 sec */
		mmc_drv.timer1 = 1000;

		/* SDv2? */
		if (send_cmd(CMD8, 0x1AA) == 0x01) {
			/* Get 32 bit return value of R7 resp */
			for (n = 0; n < 4; n++)
				ocr[n] = xchg_spi(0xFF);

			/* Is the card supports vcc of 2.7-3.6V? */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
				/* Wait for end of initialization with ACMD41(HCS) */
				while (mmc_drv.timer1 && send_cmd(ACMD41, 1UL << 30));

				/* Check CCS bit in the OCR */
				if (mmc_drv.timer1 && send_cmd(CMD58, 0) == 0) {
					for (n = 0; n < 4; n++)
						ocr[n] = xchg_spi(0xFF);
					/* Card id SDv2 */
					card_type =
						(ocr[0] & 0x40) ?
						CT_SD2 | CT_BLOCK : CT_SD2;
				}
			}
		} else { /* Not SDv2 card */
			/* SDv1 or MMC? */
			if (send_cmd(ACMD41, 0) <= 1) {
				/* SDv1 (ACMD41(0)) */
				card_type = CT_SD1; cmd = ACMD41;
			} else {
				/* MMCv3 (CMD1(0)) */
				card_type = CT_MMC; cmd = CMD1;
			}
			/* Wait for end of initialization */
			while (mmc_drv.timer1 && send_cmd(cmd, 0)) ;

			/* Set block length: 512 */
			if (!mmc_drv.timer1 || send_cmd(CMD16, 512) != 0)
				card_type = 0;
		}
	}
	mmc_drv.card_type = card_type;
	deselect();

	if (card_type != CT_UNKNOWN) {
		/* Set fast clock */
		FCLK_FAST();
		/* Clear STA_NOINIT flag */
		mmc_drv.mmc_status &= ~STA_NOINIT;
	} else {
		/* Failed */
		mmc_drv.mmc_status = STA_NOINIT;
	}

	return mmc_drv.mmc_status;
}

#if 0
/*-----------------------------------------------------------------------*/
/* Get disk status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	uint8_t drv		/* Physical drive number (0) */
)
{
	if (drv) return STA_NOINIT;		/* Supports only drive 0 */

	return mmc_drv.mmc_status;	/* Return disk status */
}



/*-----------------------------------------------------------------------*/
/* Read sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	uint8_t drv,		/* Physical drive number (0) */
	uint8_t *buff,		/* Pointer to the data buffer to store read data */
	uint32_t sector,	/* Start sector number (LBA) */
	size_t count		/* Number of sectors to read (1..128) */
)
{
	if (drv || !count) return RES_PARERR;		/* Check parameter */
	if (mmc_drv.mmc_status & STA_NOINIT) return RES_NOTRDY;	/* Check if drive is ready */

	if (!(mmc_drv.card_type & CT_BLOCK)) sector *= 512;	/* LBA ot BA conversion (byte addressing cards) */

	if (count == 1) {	/* Single sector read */
		if ((send_cmd(CMD17, sector) == 0)	/* READ_SINGLE_BLOCK */
			&& rcvr_datablock(buff, 512))
			count = 0;
	}
	else {				/* Multiple sector read */
		if (send_cmd(CMD18, sector) == 0) {	/* READ_MULTIPLE_BLOCK */
			do {
				if (!rcvr_datablock(buff, 512)) break;
				buff += 512;
			} while (--count);
			send_cmd(CMD12, 0);				/* STOP_TRANSMISSION */
		}
	}
	deselect();

	return count ? RES_ERROR : RES_OK;	/* Return result */
}



/*-----------------------------------------------------------------------*/
/* Write sector(s)                                                       */
/*-----------------------------------------------------------------------*/

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
		if ((send_cmd(CMD24, sector) == 0)	/* WRITE_BLOCK */
			&& xmit_datablock(buff, 0xFE))
			count = 0;
	}
	else {				/* Multiple sector write */
		if (mmc_drv.card_type & CT_SDC) send_cmd(ACMD23, count);	/* Predefine number of sectors */
		if (send_cmd(CMD25, sector) == 0) {	/* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD))	/* STOP_TRAN token */
				count = 1;
		}
	}
	deselect();

	return count ? RES_ERROR : RES_OK;	/* Return result */
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous drive controls other than data read/write               */
/*-----------------------------------------------------------------------*/

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
		if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
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
			if (send_cmd(ACMD13, 0) == 0) {	/* Read SD status */
				xchg_spi(0xFF);
				if (rcvr_datablock(csd, 16)) {				/* Read partial block */
					for (n = 64 - 16; n; n--) xchg_spi(0xFF);	/* Purge trailing data */
					*(uint32_t*)buff = 16UL << (csd[10] >> 4);
					res = RES_OK;
				}
			}
		} else {					/* SDC ver 1.XX or MMC */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {	/* Read CSD */
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
		if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000))	/* Erase sector block */
			res = RES_OK;	/* FatFs does not check result of this command */
		break;

	default:
		res = RES_PARERR;
	}

	deselect();

	return res;
}
#endif


/*-----------------------------------------------------------------------*/
/* Device timer function                                                 */
/*-----------------------------------------------------------------------*/
/* This function must be called from timer interrupt routine in period
/  of 1 ms to generate card control timing.
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

