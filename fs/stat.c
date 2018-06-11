#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/devices.h>
#include <cnix/fs.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>

int do_stat(const char * pathname, struct stat * statbuf)
{
	int error;
	struct inode * inoptr;

	checkname(pathname);

	inoptr = namei(pathname, &error, 0);
	if(!inoptr)
		return error;

	statbuf->st_dev = inoptr->i_dev;
	statbuf->st_ino = inoptr->i_num;
	statbuf->st_mode = inoptr->i_mode;
	statbuf->st_nlink = inoptr->i_nlinks;
	statbuf->st_uid = inoptr->i_uid;
	statbuf->st_gid = inoptr->i_gid;

	if(S_ISCHR(inoptr->i_mode) || S_ISBLK(inoptr->i_mode))
		statbuf->st_rdev = (dev_t)inoptr->i_zone[0];

	statbuf->st_size = inoptr->i_size;

	statbuf->st_blksize = BLOCKSIZ;
	statbuf->st_blocks = calc_block_nr(inoptr->i_size);

	statbuf->st_atime = inoptr->i_atime;
	statbuf->st_atime_nsec = 0;
	statbuf->st_mtime = inoptr->i_mtime;
	statbuf->st_mtime_nsec = 0;
	statbuf->st_ctime = inoptr->i_ctime;
	statbuf->st_ctime_nsec = 0;

	iput(inoptr);

	return 0;
}

int do_lstat(const char * pathname, struct stat * statbuf)
{
	int error = 0;
	struct inode * inoptr;

	checkname(pathname);

	inoptr = namei0(pathname, &error, 0);
	if(!inoptr)
		return error;

	statbuf->st_dev = inoptr->i_dev;
	statbuf->st_ino = inoptr->i_num;
	statbuf->st_mode = inoptr->i_mode;
	statbuf->st_nlink = inoptr->i_nlinks;
	statbuf->st_uid = inoptr->i_uid;
	statbuf->st_gid = inoptr->i_gid;

	if(S_ISCHR(inoptr->i_mode) || S_ISBLK(inoptr->i_mode))
		statbuf->st_rdev = (dev_t)inoptr->i_zone[0];

	statbuf->st_size = inoptr->i_size;

	statbuf->st_blksize = BLOCKSIZ;
	statbuf->st_blocks = calc_block_nr(inoptr->i_size);

	statbuf->st_atime = inoptr->i_atime;
	statbuf->st_atime_nsec = 0;
	statbuf->st_mtime = inoptr->i_mtime;
	statbuf->st_mtime_nsec = 0;
	statbuf->st_ctime = inoptr->i_ctime;
	statbuf->st_ctime_nsec = 0;

	iput(inoptr);

	return 0;
}

int do_fstat(int fd, struct stat * statbuf)
{
	struct filp * fp;
	struct inode * inoptr;

	fp = fget(fd);
	if(!fp)
		DIE("BUG: cannot happen\n");

	inoptr = fp->f_ino;

	statbuf->st_dev = inoptr->i_dev;
	statbuf->st_ino = inoptr->i_num;

	if(inoptr->i_flags & PIPE_I)
		statbuf->st_mode = I_FIFO | 0777;
	else
		statbuf->st_mode = inoptr->i_mode;

	statbuf->st_nlink = inoptr->i_nlinks;
	statbuf->st_uid = inoptr->i_uid;
	statbuf->st_gid = inoptr->i_gid;

	if(S_ISCHR(inoptr->i_mode) || S_ISBLK(inoptr->i_mode))
		statbuf->st_rdev = (dev_t)inoptr->i_zone[0];

	statbuf->st_size = inoptr->i_size;

	statbuf->st_blksize = BLOCKSIZ;
	statbuf->st_blocks = calc_block_nr(inoptr->i_size);

	statbuf->st_atime = inoptr->i_atime;
	statbuf->st_atime_nsec = 0;
	statbuf->st_mtime = inoptr->i_mtime;
	statbuf->st_mtime_nsec = 0;
	statbuf->st_ctime = inoptr->i_ctime;
	statbuf->st_ctime_nsec = 0;

	return 0;
}
