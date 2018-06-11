#ifndef _IDE_H
#define _IDE_H

#include <cnix/wait.h>

#define IDE_DATA	0x1f0
#define IDE_ERROR	0x1f1
#define IDE_NSECTOR	0x1f2
#define IDE_SECTOR	0x1f3
#define IDE_LCYL	0x1f4	
#define IDE_HCYL	0x1f5
#define IDE_CURRENT	0x1f6
#define IDE_STATUS	0x1f7
#define IDE_CTL		0x3f6

#define IDE_PRECOMP	IDE_ERROR
#define IDE_CMD		IDE_STATUS

/* IDE_STATUS */
#define ERR_STAT	0x01
#define INDEX_STAT	0x02
#define ECC_STAT	0x04
#define DRQ_STAT	0x08
#define SEEK_STAT	0x10
#define WRERR_STAT	0x20
#define READY_STAT	0x40
#define BUSY_STAT	0x80

/* IDE_CMD */
#define IDLE_CMD	0x00
#define RECALIBRATE_CMD	0x10
#define READ_CMD	0x20
#define WRITE_CMD	0x30
#define READVERIFY_CMD	0x40
#define FORMAT_CMD	0x50
#define SEEK_CMD	0x70
#define DIAG_CMD	0x90
#define SPECIFY_CMD	0x91
#define ATA_IDENTIFY	0xec

#define DMA_READ_CMD	0xc8
#define DMA_WRITE_CMD	0xca

/* IDE_CTL */
#define NORETRY_CTL	0x80
#define NOECC_CTL	0x40
#define EIGHTHEADS_CTL	0x08
#define RESET_CTL	0x04
#define INTDISABLE_CTL	0x02

#define TIMEOUT		32000

#define REQQ_SIZE	1024

#endif 
