#ifndef _PARTITION_H
#define _PARTITION_H

struct partition {
	unsigned char boot_ind;		/* 0x80 - active (unused) */
	unsigned char head;		/* ? */
	unsigned char sector;		/* ? */
	unsigned char cyl;		/* ? */
	unsigned char sys_ind;		/* ? */
	unsigned char end_head;		/* ? */
	unsigned char end_sector;	/* ? */
	unsigned char end_cyl;		/* ? */
	unsigned int start_sec;		/* starting sector counting from 0 */
	unsigned int num_sec;		/* nr of sectors in partition */
};

#endif
