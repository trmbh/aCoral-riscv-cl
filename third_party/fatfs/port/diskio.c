/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "sdcard.h"
#include "sysctl.h"        /* For K210 system control */
#include "dmac.h"          /* For K210 DMA control */
#include <stdbool.h>       /* For bool type */

/* Definitions of physical drive number for each drive */
#define SD_CARD        0    /* Map SD card to physical drive 0 */

/* Timer variables */
static volatile BYTE Timer1;
static volatile BYTE Timer2;

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status(BYTE pdrv)
{
	if (pdrv != SD_CARD) return STA_NOINIT;
	return 0;  // Always return OK status once initialized
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(BYTE pdrv)
{
	if (pdrv != SD_CARD) return STA_NOINIT;

	// Enable required clocks
	sysctl_clock_enable(SYSCTL_CLOCK_SPI0);
	sysctl_clock_enable(SYSCTL_CLOCK_DMA);

	// Initialize SD card
	if (sd_init() == 0)
		return 0;
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
	if (pdrv != SD_CARD) return RES_PARERR;
	
	// Use DMA for large transfers, normal read for small ones
	if (count > 1) {
		if (sd_read_sector_dma(buff, sector, count) == 0)
			return RES_OK;
	} else {
		if (sd_read_sector(buff, sector, count) == 0)
			return RES_OK;
	}
	return RES_ERROR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
	if (pdrv != SD_CARD) return RES_PARERR;

	// Use DMA for large transfers, normal write for small ones
	if (count > 1) {
		if (sd_write_sector_dma((BYTE *)buff, sector, count) == 0)
			return RES_OK;
	} else {
		if (sd_write_sector((BYTE *)buff, sector, count) == 0)
			return RES_OK;
	}
	return RES_ERROR;
}



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
	DRESULT res = RES_ERROR;

	if (pdrv != SD_CARD) return RES_PARERR;

	switch (cmd) {
	/* Make sure that no pending write process */
	case CTRL_SYNC:
		res = RES_OK;
		break;
	/* Get number of sectors on the disk (DWORD) */
	case GET_SECTOR_COUNT:
		*(DWORD *)buff = (cardinfo.SD_csd.DeviceSize + 1) << 10;
		res = RES_OK;
		break;
	/* Get R/W sector size (WORD) */
	case GET_SECTOR_SIZE:
		*(WORD *)buff = cardinfo.CardBlockSize;
		res = RES_OK;
		break;
	/* Get erase block size in unit of sector (DWORD) */
	case GET_BLOCK_SIZE:
		*(DWORD *)buff = cardinfo.CardBlockSize;
		res = RES_OK;
		break;
	case CTRL_TRIM:        /* Inform device that the data on the block of sectors is no longer used */
		res = RES_OK;
		break;
	default:
		res = RES_PARERR;
	}
	return res;
}

// Additional functions for K210

/*-----------------------------------------------------------------------*/
/* Device Timer Interrupt Procedure                                        */
/*-----------------------------------------------------------------------*/
void disk_timerproc(void)
{
	BYTE n;

	n = Timer1;                /* 100Hz decrement timer */
	if (n) Timer1 = --n;
	n = Timer2;
	if (n) Timer2 = --n;
}
