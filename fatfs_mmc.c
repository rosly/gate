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
#include "crc_itu.h"

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

#ifdef _VERBOSE_DEBUG
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
#endif

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
static void spi_rcvr_multi(uint8_t *buff, size_t cnt)
{
	size_t rx = 0;

	if (cnt == 0)
		return;

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
}

#if _USE_WRITE
/** 
 * Send multiple byte
 *
 * @param buff - Pointer to the data
 * @param cnt - Number of bytes to send (even number)
 */
static void spi_xmit_multi(const uint8_t *buff, size_t cnt)
{
	size_t tx = 0;

	if (cnt == 0)
		return;

	/* transmiter will start SPI clock after write operation to TXD register
	 * TXD register is double buffered (same RXD) so we put two bytes there in
	 * serie. Start transmiting first byte */
	nrf_spi_txd_set(MMC_SPI, buff[tx++]);

	/* next, xmit and receive required amout of bytes */
	while (cnt > tx) {
		nrf_spi_txd_set(MMC_SPI, buff[tx++]);

		/* wait until xmit and receive is finished */
		while (!nrf_spi_event_check(MMC_SPI, NRF_SPI_EVENT_READY));
		nrf_spi_event_clear(MMC_SPI, NRF_SPI_EVENT_READY);
		/* we must read the RX register even if we will not use the recv byte */
		(void)nrf_spi_rxd_get(MMC_SPI);
	}

	/* finally, wait and receive te final byte which was cheduled by double
	 * bufered operation. In case cnt == 1 we will not execute the upper loop */
	while (!nrf_spi_event_check(MMC_SPI, NRF_SPI_EVENT_READY));
	nrf_spi_event_clear(MMC_SPI, NRF_SPI_EVENT_READY);
	/* we must read the RX register even if we will not use the recv byte */
	(void)nrf_spi_rxd_get(MMC_SPI);
}
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

/**
 * Receive the data block from card
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
	crc_cal = crc_itu(0x0000, buff, buff_size);

	/* compare CRC's */
	if (crc_cal != crc_rcv) {
		log_printf("CRC error!");
		return 0; /* Error */
	}

	return 1; /* Success */
}

#if _USE_WRITE
/**
 * Transmit the data block to card
 *
 * @param buff - Ponter to 512 byte data to be sent
 * @param token - Token ????
 *
 * @return :OK, 0:Failed
 */
static int card_xmit_datablock(const uint8_t *buff, uint8_t token)
{
	uint8_t resp;

	/* Wait for card ready */
	if (!card_wait_ready(500))
		return 0;	

	/* Send token */
	spi_xchg(token);					

	/* Send data if token is other than StopTran */
	if (token != 0xFD) {	
		spi_xmit_multi(buff, 512); /* Data */
		spi_xchg(0xFF); spi_xchg(0xFF); /* Dummy CRC */

		/* Receive data resp */
		resp = spi_xchg(0xFF);		
		/* Function fails if the data packet was not accepted */
		if ((resp & 0x1F) != 0x05)	
			return 0;
	}
	return 1;
}
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

	/* Allow only for single initialization */
	if (!(mmc_drv.mmc_status & STA_NOINIT))
		return mmc_drv.mmc_status;

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

#ifdef _VERBOSE_DEBUG
		/* Verify the speed settings */
		spi_check_delay();
#endif

		/* Clear STA_NOINIT flag */
		mmc_drv.mmc_status &= ~STA_NOINIT;
	} else {
		/* Failed */
		mmc_drv.mmc_status = STA_NOINIT;
	}

	return mmc_drv.mmc_status;
}

/** Function returns disk status
 *
 * @param drv - Physical drive number (0)
 * #return Disk status
 */
DSTATUS disk_status(uint8_t drv)
{
	/* Supports only drive 0 */
	if (drv > 0)
		return STA_NOINIT;

	/* Return disk status */
	return mmc_drv.mmc_status;
}

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

/**
 * Write sector(s)
 *
 * @param drv - Physical drive number (0)
 * @param buff - Ponter to the data to write
 * @param sector - Start sector number (LBA)
 * @param count - Number of sectors to write (1..128)
 *
 * @return - Result of operation
 */
#if _USE_WRITE
DRESULT disk_write(uint8_t drv, const uint8_t *buff, uint32_t sector, size_t count)
{
	/* Check parameter  - only one disk allowed */
	if (drv || !count)
		return RES_PARERR;

	/* Check drive status */
	if (mmc_drv.mmc_status & STA_NOINIT) 
		return RES_NOTRDY;	

	/* Check write protect */
	if (mmc_drv.mmc_status & STA_PROTECT)
		return RES_WRPRT;

	/* LBA ==> BA conversion for byte addressing cards */
	if (!(mmc_drv.card_type & CT_BLOCK))
		sector *= 512;	

	if (count == 1) {
		/* Single sector write */
		if ((card_send_cmd(CMD24, sector) == 0) &&
			  card_xmit_datablock(buff, 0xFE))
				count = 0;
	}
	else {			
		/* Multiple sector write */
		/* Predefine number of sectors */
		if (mmc_drv.card_type & CT_SDC) 
			card_send_cmd(ACMD23, count);	

		if (card_send_cmd(CMD25, sector) == 0) {
			do {
				if (!card_xmit_datablock(buff, 0xFC))
					break; /* Error */
				buff += 512;
			} while (--count);

			/* STOP_TRAN token */
			if (!card_xmit_datablock(0, 0xFD))	
				count = 1;
		}
	}
	card_deselect();

	return count ? RES_ERROR : RES_OK;	/* Return result */
}
#endif

/** Miscellaneous drive controls other than data read/write
 * @param drv - Physical drive number (0)
 * @param cmd - Control command code
 * @param buff - Pointer to the conrtol data
 * @return - result of operation */
#if _USE_IOCTL
DRESULT disk_ioctl(uint8_t drv, uint8_t cmd, void *buff)
{
	DRESULT res;
	uint8_t n, csd[16];
	uint32_t *dp, st, ed, csize;

	/* Check parameter - only one disk */
	if (drv > 0)
		return RES_PARERR;

	/* Check if drive is ready */
	if (mmc_drv.mmc_status & STA_NOINIT)
		return RES_NOTRDY;

	res = RES_ERROR;

	switch (cmd) {
	case CTRL_SYNC :
		/* Wait for end of internal write process of the drive */
		if (card_select())
			res = RES_OK;
		break;

	case GET_SECTOR_COUNT :
		/* Get drive capacity in unit of sector (uint32_t) */
		/* Read CSD */
		if ((card_send_cmd(CMD9, 0) == 0) && card_rcv_block(csd, 16)) {
			if ((csd[0] >> 6) == 1) {
				/* SDC ver 2.00 */
				csize = csd[9] + ((WORD)csd[8] << 8) + ((uint32_t)(csd[7] & 63) << 16) + 1;
				*(uint32_t*)buff = csize << 10;
			} else {
				/* SDC ver 1.XX or MMC ver 3 */
				n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
				csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
				*(uint32_t*)buff = csize << (n - 9);
			}
			res = RES_OK;
		}
		break;

	case GET_BLOCK_SIZE :
		/* Get erase block size in unit of sector (uint32_t) */
		if (mmc_drv.card_type & CT_SD2) {
			/* SDC ver 2.00 */
			if (card_send_cmd(ACMD13, 0) == 0) {
				/* Read SD status */
				spi_xchg(0xFF);
				/* Read partial block */
				if (card_rcv_block(csd, 16)) {
					/* Purge trailing data */
					for (n = 64 - 16; n; n--)
						spi_xchg(0xFF);
					*(uint32_t*)buff = 16UL << (csd[10] >> 4);
					res = RES_OK;
				}
			}
		} else {
			/* SDC ver 1.XX or MMC */
			/* Read CSD */
			if ((card_send_cmd(CMD9, 0) == 0) && card_rcv_block(csd, 16)) {
				if (mmc_drv.card_type & CT_SD1) {
					/* SDC ver 1.XX */
					*(uint32_t*)buff = (((csd[10] & 63) << 1) +
						((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
				} else {
					/* MMC */
					*(uint32_t*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) *
						(((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
				}
				res = RES_OK;
			}
		}
		break;

	case CTRL_TRIM :
		/* Erase a block of sectors (used when _USE_ERASE == 1) */
		/* Check if the card is SDC */
		if (!(mmc_drv.card_type & CT_SDC))
			break;
		/* Get CSD */
		if (disk_ioctl(drv, MMC_GET_CSD, csd))
			break;
		/* Check if sector erase can be applied to the card */
		if (!(csd[0] >> 6) && !(csd[10] & 0x40))
			break;
		/* Load sector block */
		dp = buff;
		st = dp[0];
		ed = dp[1];
		if (!(mmc_drv.card_type & CT_BLOCK)) {
			st *= 512; ed *= 512;
		}
		/* Erase sector block */
		if (card_send_cmd(CMD32, st) == 0 &&
		    card_send_cmd(CMD33, ed) == 0 &&
		    card_send_cmd(CMD38, 0) == 0 &&
		    card_wait_ready(30000))
			res = RES_OK;	/* FatFs does not check result of this command */
		break;

	default:
		res = RES_PARERR;
	}

	card_deselect();

	return res;
}
#endif

#if 0
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

