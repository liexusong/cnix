#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/devices.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>

int tobits[3] = { I_RB, I_WB, I_RB | I_WB };

int do_creat(const char * filename, mode_t mode)
{
	return do_open(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

/*
 * assume that filename has been checked in sys_open
 */
int do_open(const char * filename, int flags, mode_t mode)
{
	int error = 0, fd, major, minor;
	struct inode * inoptr, * fino;
	mode_t mode1;

	checkname(filename);

	inoptr = namei(filename, &error, 0);

	if(flags & O_CREAT){
		if(inoptr){
			if((flags & O_EXCL) && !S_ISCHR(inoptr->i_mode)){
				iput(inoptr);
				return -EEXIST;
			}
		}else{
			char part[NAME_MAX + 1];

			fino = namei(filename, &error, 1);
			if(!fino)
				return error;

			if(!S_ISDIR(fino->i_mode)){
				iput(fino);
				return -ENOTDIR;
			}

			if(!get_tail(filename, part)){
				iput(fino);
				return -ENAMETOOLONG;
			}

			if(admit(fino, I_WB) == 0){
				iput(fino);
				return -EACCES;
			}

			mode1 = ((mode & I_UGRWX) & (~(current->umask))) | I_REG;
			inoptr = iinsert(fino, part, mode1, &error, 0);

			iput(fino);

			if(!inoptr)
				return error;
		}
	}

	if(!inoptr)
		return -ENOENT;

	if(admit(inoptr, tobits[flags & O_ACCMODE]) == 0){
		error = -EACCES;
		goto errout;
	}

	if(S_ISFIFO(inoptr->i_mode)){
		error = -ENOTSUP; /* XXX */
		goto errout;
	}else if(S_ISDIR(inoptr->i_mode) && ((flags & O_ACCMODE) != O_RDONLY)){
		/* only can open a directoy for reading */
		error = -EISDIR;
		goto errout;
	}

	/* if O_TRUNC is set, and allows to write */
	if((flags & O_TRUNC) && (flags & O_ACCMODE) && S_ISREG(inoptr->i_mode))
		truncate(inoptr);

	/* block device or char device */
	if(S_ISBLK(inoptr->i_mode) || S_ISCHR(inoptr->i_mode)){
		major = MAJOR(inoptr->i_zone[0]);
		minor = MINOR(inoptr->i_zone[0]);

		if(major >= get_dev_nr() || !dev_table[major].open){
			error = -ENODEV;
			goto errout;
		}

		error = dev_table[major].open(minor, flags);
		if(error < 0)
			goto errout;
	}

	fd = fdget(0, flags, inoptr, &error);
	if(fd < 0){
		iput(inoptr);
		return error;
	}

	if(flags & O_APPEND){
		struct filp * fp;

		fp = fget(fd);
		fp->f_pos = inoptr->i_size;
	}

	inoptr->i_update |= ATIME;
	inoptr->i_dirty = 1;

	inoptr->i_count++;

	iput(inoptr);

	return fd;

errout:
	iput(inoptr);

	return error;
}

int do_inode_open(struct inode * inoptr, int flags)
{
	int fd, error = 0;

	fd = fdget(0, flags, inoptr, &error);
	if(fd < 0)
		return error;

	if(flags & O_APPEND){
		struct filp * fp;

		fp = fget(fd);
		fp->f_pos = inoptr->i_size;
	}

	inoptr->i_update |= ATIME;
	inoptr->i_dirty = 1;

	inoptr->i_count++;

	return fd;
}

int do_mknod(const char * filename, mode_t mode, dev_t dev)
{
	int error = 0;
	struct inode * inoptr, * fino;
	char part[NAME_MAX + 1];
	mode_t mode1;

	checkname(filename);

	fino = namei(filename, &error, 1);
	if(!fino)
		return error;

	if(!S_ISDIR(fino->i_mode)){
		iput(fino);
		return -ENOTDIR;
	}

	if(admit(fino, I_WB) == 0){
		iput(fino);
		return -EACCES;
	}

	if(!get_tail(filename, part)){
		iput(fino);
		return -ENAMETOOLONG;
	}

	/* make sure won't be totally iput by ifind */
	fino->i_count++;

	inoptr = ifind(fino, part);
	if(inoptr){
		iput(inoptr);
		iput(fino);
		return -EEXIST;
	}

	ilock(fino);

	mode1 = ((mode & I_UGRWX) & (~(current->umask))) |  (mode & I_TYPE);
	inoptr = iinsert(fino, part, mode1, &error, 0);

	if(!inoptr){
		iput(fino);
		return error;
	}

	iput(fino);	

	inoptr->i_zone[0] = dev; /* XXX */
	iput(inoptr);

	return 0;
}
