#ifndef _INODE_T
#define _INODE_T

#include <cnix/types.h>
#include <cnix/list.h>
#include <cnix/select.h>

#define NR_ZONES	10
#define NR_DZONES	7
#define NR_INDIRECTS	(BLOCKSIZ / sizeof(zone_t))

#define LZSIZE		0
#define ZONE_SIZE	(BLOCKSIZ << LZSIZE)

/* assume zone size is the same as block size */
#define FILESIZE_MAX	\
	(NR_DZONES * ZONE_SIZE + NR_INDIRECTS * ZONE_SIZE \
	+ NR_INDIRECTS * NR_INDIRECTS * ZONE_SIZE)

#define NR_INODE	2048
#define NR_INODE_HASH	(NR_INODE / 32)	/* average 32 */

#define ROOT_INODE	1

struct super_block;
struct wait_queue;

struct inode{
	mode_t	i_mode;
	nlink_t	i_nlinks;	/* will be paded to 2 bytes */
	uid_t	i_uid;
	gid_t	i_gid;		/* will be paded to 2 bytes */
	off_t	i_size;
	time_t	i_atime;
	time_t	i_mtime;
	time_t	i_ctime;
	zone_t	i_zone[NR_ZONES];

	/* in memory */
	dev_t	i_dev;		/* be sure it's the first of below */
	
	int	i_count;	/* 0 indicate free */
	int	i_dirty;

	unsigned char i_update;

	list_t i_hashlist;
	list_t i_freelist;

	unsigned short i_flags;		/* LOCK WANT PIPE MOUNT SEEK */
	pid_t i_lockowner;
	struct wait_queue * i_wait;	/* if WANT */

	union{
		struct{
			ino_t	i_num;
			struct super_block * i_sp;
			struct super_block * i_mounts;	/* if MOUNT */
			int	i_ndzones;
			int	i_nindirs;
		}s1;

		struct{
			int i_buddy;			/* READBLE, WRITABLE */
			unsigned char * i_buffer;	/* for pipe */
			short i_readp;
			short i_writep;
			select_t i_select;
		}s2;

		void * i_data; // for others
	}u;
};

#define i_num		u.s1.i_num
#define i_sp		u.s1.i_sp
#define i_mounts 	u.s1.i_mounts
#define i_ndzones	u.s1.i_ndzones
#define i_nindirs	u.s1.i_nindirs

#define i_buddy		u.s2.i_buddy
#define i_buffer	u.s2.i_buffer
#define i_readp		u.s2.i_readp
#define i_writep	u.s2.i_writep
#define i_select	u.s2.i_select

#define i_data		u.i_data

/* bits for i_mode */
#define I_TYPE	0170000	/* inode type mode */
#define I_SOCK	0140000 /* socket */
#define I_LNK	0120000 /* symbolic link */
#define I_REG	0100000	/* regular file */
#define I_BLK	0060000	/* block special file */
#define I_DIR	0040000	/* directory */
#define I_CHR	0020000	/* character special file */
#define I_FIFO	0010000 /* named pipe */

#define S_ISSOCK(m)	((m & I_TYPE) == I_SOCK)
#define S_ISLNK(m)	((m & I_TYPE) == I_LNK)
#define S_ISREG(m)	((m & I_TYPE) == I_REG)
#define S_ISBLK(m)	((m & I_TYPE) == I_BLK)
#define S_ISDIR(m)	((m & I_TYPE) == I_DIR)
#define S_ISCHR(m)	((m & I_TYPE) == I_CHR)
#define S_ISFIFO(m)	((m & I_TYPE) == I_FIFO)

#define I_SU	0004000	/* set effective uid on exec */
#define I_SG	0002000	/* set effective gid on exec */

#define I_UGRWX	0006777

#define I_RWX	0000777
#define I_RB	0000004
#define I_WB	0000002
#define I_XB	0000001
#define I_FREE	0000000

#define MOUNT_I	01
#define PIPE_I	02
#define SEEK_I	04
#define LOCK_I	010
#define WANT_I	020

#define ATIME	01 /* change access time */
#define MTIME	02 /* change modify time */
#define CTIME	04 /* change create time */

#define INODE_SIZE	((int)(&(((struct inode *)(0))->i_dev)))

extern struct inode * iget(dev_t, ino_t);
extern void iput(struct inode *);
extern struct inode * ialloc(dev_t, mode_t, int *);
extern void ilock(struct inode *);
extern int get_iiucount(dev_t);

extern int tobits[3];

#endif
