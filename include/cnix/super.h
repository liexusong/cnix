#ifndef _SUPER_H
#define _SUPER_H

#include <cnix/types.h>
#include <cnix/inode.h>

struct super_block{
	ino_t	s_ninodes;	/* total inodes'number */
	zone1_t	s_nzones;	/* V1 total zones'number, include bitmap */
	short	s_imblocks;	/* inode map blocks */
	short	s_zmblocks;	/* zone map blocks */
	zone1_t	s_fdzone;	/* first data zone number */
	short	s_lzsize;	/* log of zone size */
	off_t	s_msize;	/* max file size */
	short	s_magic;	/* magic number */
	short	s_pad;		/* padding */
	zone_t	s_zones;	/* V2 total zones'number, include bitmap */

	/* in memory */
	struct inode * s_rooti;	 /* its root inode */
	struct inode * s_mounti; /* the inode it's mounted on if it's mounted */

	int s_count;		/* increment in mount, decrement in umount */
	unsigned short s_flags;	/* LOCK_S, WANT_S, LOCK_BITOP, WANT_BITOP */
       	struct wait_queue * s_wait, * s_bitop_wait;

	dev_t s_dev;

	int s_roflag;
	bit_t s_isearch;
	bit_t s_zsearch;

	int s_version;
	unsigned s_ipblock;	/* inodes'number per block */

	int s_ndzones;
	int s_nindirs;
};

#define LOCK_S		01
#define WANT_S		02
#define LOCK_BITOP	04
#define WANT_BITOP	010

#define NOBIT	0

#define IMAP	0
#define ZMAP	1

extern void init_super(void);
extern bit_t bitalloc(struct super_block *, int);
extern void bitfree(struct super_block *, bit_t, int);
extern void sfree(struct super_block *);
extern struct super_block * sread(dev_t);
extern struct super_block * get_sb(dev_t dev);
extern int get_free_bits_nr(struct super_block *sp, int *pcount, int which);
#endif
