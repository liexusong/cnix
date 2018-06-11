#ifndef _STAT_H
#define _STAT_H

#include <cnix/types.h>

struct kernel_old_stat{
	dev_t		st_dev;
	ino_t		st_ino;
	mode_t		st_mode;
	nlink_t		st_nlink;
	uid_t		st_uid;
	gid_t		st_gid;
	dev_t		st_rdev;
	off_t		st_size;
	long		st_blksize;
	long		st_blocks;
	time_t		st_atime;
	time_t		st_mtime;
	time_t		st_ctime;
};

/* confirm to linux newstat's struct, __NR_stat = 106 */
struct stat
{
	unsigned long  st_dev;
	unsigned long  st_ino;
	unsigned short st_mode;
	unsigned short st_nlink;
	unsigned short st_uid;
	unsigned short st_gid;
	unsigned long  st_rdev;
	unsigned long  st_size;
	unsigned long  st_blksize;
	unsigned long  st_blocks;
	unsigned long  st_atime;
	unsigned long  st_atime_nsec;
	unsigned long  st_mtime;
	unsigned long  st_mtime_nsec;
	unsigned long  st_ctime;
	unsigned long  st_ctime_nsec;
	unsigned long  __unused4;
	unsigned long  __unused5;
};

typedef struct { int __val[2]; } __fsid_t;

struct statfs
{
	int f_type;
	int f_bsize;

	unsigned long f_blocks;
	unsigned long f_bfree;
	unsigned long f_bavail;
	unsigned long f_files;
	unsigned long f_ffree;

	__fsid_t f_fsid;
	ssize_t f_namelen;
	ssize_t f_frsize;
	ssize_t f_spare[5];
};

#endif
