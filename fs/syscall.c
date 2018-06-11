#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/stat.h>
#include <cnix/config.h>
#include <cnix/mm.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>
#include <cnix/devices.h>
#include <cnix/sockios.h>
#include <cnix/ioctl.h>
#include <cnix/syslog.h>

extern int do_with_netif_ioctl(int request, void *data);

char * getname(const char * pathname)
{
	int len;
	char * tmp;

	len = (unsigned long)KERNEL_ADDR - (unsigned long)pathname;
	if((unsigned long)len > (unsigned long)PATH_MAX)
		len = PATH_MAX; /* not include '\0' */

	/* XXX: PATH_MAX  + 1 < PAGE_SIZE */
	tmp = (char *)get_one_page();
	if(!tmp)
		return NULL;

	strncpy(tmp, pathname, len);
	tmp[len] = '\0';
	
	return tmp;
}

void freename(char * name)
{
	free_one_page((unsigned long)name);
}

int sys_creat(const char * pathname, mode_t mode)
{
	int ret;
	char * tmp;
			
	if(cklimit(pathname))
		return -EFAULT;

	sys_checkname(pathname);

	tmp = getname(pathname);
	if(!tmp)
		return -EAGAIN;

	ret = do_creat(tmp, mode);

	freename(tmp);

	return ret;
}

int sys_open(const char * pathname, int flags, mode_t mode)
{
	int ret;
	char * tmp;

	if(cklimit(pathname))
		return -EFAULT;

	sys_checkname(pathname);

	tmp = getname(pathname);
	if(!tmp)
		return -EAGAIN;

	ret = do_open(tmp, flags, mode);

	freename(tmp);

	return ret;
}

int sys_mknod(const char * pathname, mode_t mode, dev_t dev)
{
	int ret;
	char * tmp;

	if(cklimit(pathname))
		return -EFAULT;

	sys_checkname(pathname);

	tmp = getname(pathname);
	if(!tmp)
		return -EAGAIN;

	ret = do_mknod(tmp, mode, dev);

	freename(tmp);

	return ret;
}

int sys_close(int fd)
{
	int ret;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(!fget(fd))
		return -EBADF;

	ret = do_close(fd);

	return ret;
}

ssize_t sys_read(int fd, char * buffer, size_t size)
{
	int ret;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(!fget(fd))
		return -EBADF;

	if((size < 0) || !buffer)
		return -EINVAL;

	if(cklimit(buffer) || ckoverflow(buffer, size))
		return -EFAULT;

	ret = do_read(fd, buffer, size);

	return ret;
}

int sys_lseek(int fd, off_t offset, int whence)
{
	int ret;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(!fget(fd))
		return -EBADF;

	ret = do_lseek(fd, offset, whence);

	return ret;
}

ssize_t sys_write(int fd, char * buffer, size_t size)
{
	int ret;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(!fget(fd))
		return -EBADF;

	if((size < 0) || !buffer)
		return -EINVAL;

	if(cklimit(buffer) || ckoverflow(buffer, size))
		return -EFAULT;

	ret = do_write(fd, buffer, size);

	return ret;
}

int sys_stat(const char * pathname, struct stat * statbuf)
{
	int ret;
	char * tmp;

	if(cklimit(pathname) 
		|| cklimit(statbuf) || ckoverflow(statbuf, sizeof(struct stat)))
		return -EFAULT;

	sys_checkname(pathname);

	tmp = getname(pathname);
	if(!tmp)
		return -EAGAIN;

	ret = do_stat(tmp, statbuf);

	freename(tmp);

	return ret;
}

int sys_lstat(const char * pathname, struct stat * statbuf)
{
	int ret;
	char * tmp;

	if(cklimit(pathname) 
		|| cklimit(statbuf) || ckoverflow(statbuf, sizeof(struct stat)))
		return -EFAULT;

	sys_checkname(pathname);

	tmp = getname(pathname);
	if(!tmp)
		return -EAGAIN;

	ret = do_lstat(tmp, statbuf);

	freename(tmp);

	return ret;
}

int sys_fstat(int fd, struct stat * statbuf)
{
	int ret;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(!fget(fd))
		return -EBADF;

	if(cklimit(statbuf) || ckoverflow(statbuf, sizeof(struct stat)))
		return -EFAULT;

	ret = do_fstat(fd, statbuf);

	return ret;
}

int sys_dup(int fd)
{
	int ret;

	ret = do_dup(fd);

	return ret;
}

int sys_dup2(int oldfd, int newfd)
{
	int ret;

	ret = do_dup2(oldfd, newfd);

	return ret;
}

int sys_link(const char * srcname, const char * destname)
{
	int ret;
	char * tmp1, * tmp2;

	if(cklimit(srcname) || cklimit(destname))
		return -EFAULT;

	sys_checkname(srcname);
	sys_checkname(destname);

	tmp1 = getname(srcname);
	if(!tmp1)
		return -EAGAIN;

	tmp2 = getname(destname);
	if(!tmp2){
		freename(tmp1);
		return -EAGAIN;
	}

	ret = do_link(tmp1, tmp2);

	freename(tmp1);
	freename(tmp2);

	return ret;
}

int sys_unlink(char * pathname)
{
	int ret;
	char * tmp;
	
	if(cklimit(pathname))
		return -EFAULT;

	sys_checkname(pathname);

	tmp = getname(pathname);
	if(!tmp)
		return -EAGAIN;

	ret = do_unlink(tmp);

	freename(tmp);
	
	return ret;
}

int sys_rename(const char * oldpath, const char * newpath)
{
	int ret;
	char * tmp1, * tmp2;

	if(cklimit(oldpath) || cklimit(newpath))
		return -EFAULT;

	sys_checkname(oldpath);
	sys_checkname(newpath);

	tmp1 = getname(oldpath);
	if(!tmp1)
		return -EAGAIN;

	tmp2 = getname(newpath);
	if(!tmp2){
		freename(tmp1);
		return -EAGAIN;
	}

	ret = do_rename(tmp1, tmp2);

	freename(tmp1);
	freename(tmp2);

	return ret;
}

int sys_symlink(const char * oldpath, const char * newpath)
{
	int ret;
	char * tmp1, * tmp2;

	if(cklimit(oldpath) || cklimit(newpath))
		return -EFAULT;

	sys_checkname(oldpath);
	sys_checkname(newpath);

	tmp1 = getname(oldpath);
	if(!tmp1)
		return -EAGAIN;

	tmp2 = getname(newpath);
	if(!tmp2){
		freename(tmp1);
		return -EAGAIN;
	}

	ret = do_symlink(tmp1, tmp2);

	freename(tmp1);
	freename(tmp2);

	return ret;
}

int sys_readlink(const char * path, char * buff, size_t buf_size)
{
	int ret;
	char * tmp;

	if(buf_size < 0)
		return -EINVAL;

	if(cklimit(path) || ckoverflow(buff, buf_size))
		return -EFAULT;

	sys_checkname(path);

	tmp = getname(path);
	if(!tmp)
		return -EAGAIN;

	ret = do_readlink(tmp, buff, buf_size);

	freename(tmp);

	return ret;
}

int sys_mount(const char * specfile, const char * dirname, int rwflag)
{
	int ret;
	char * tmp1, * tmp2;

	if(cklimit(specfile) || cklimit(dirname))
		return -EFAULT;

	sys_checkname(specfile);
	sys_checkname(dirname);

	tmp1 = getname(specfile);
	if(!tmp1)
		return -EAGAIN;

	tmp2 = getname(dirname);
	if(!tmp2){
		freename(tmp1);
		return -EAGAIN;
	}

	ret = do_mount(tmp1, tmp2, rwflag);

	freename(tmp1);
	freename(tmp2);

	return ret;
}

int sys_umount(const char * specialfile)
{
	int ret;
	char * tmp;

	if(cklimit(specialfile))
		return -EFAULT;

	sys_checkname(specialfile);

	tmp = getname(specialfile);
	if(!tmp)
		return -EAGAIN;

	ret = do_umount(tmp);

	freename(tmp);

	return ret;
}

int sys_chdir(const char * pathname)
{
	int ret;
	char * tmp;

	if(cklimit(pathname))
		return -EFAULT;

	sys_checkname(pathname);

	tmp = getname(pathname);
	if(!tmp)
		return -EAGAIN;

	ret = do_chdir(tmp);

	freename(tmp);
	
	return ret;
}

int sys_fchdir(int fd)
{
	struct filp *fp;
	struct inode *inoptr;
	struct inode *pwd;

	if(ckfdlimit(fd))
		return -EINVAL;

	fp = fget(fd);
	if (!fp)
		return -EBADF;

	inoptr = fp->f_ino;
	ilock(inoptr);
	inoptr->i_count++;

	if (S_ISDIR(inoptr->i_mode) == 0)
	{
		iput(inoptr);
		return -ENOTDIR;
	}

	/* XXX */
	if (admit(inoptr, I_RB) == 0)
	{
		iput(inoptr);
		return -EPERM;
	}

	pwd = current->pwd;
	if (pwd == inoptr)
	{
		iput(inoptr);
		return 0;
	}
	
	iput(pwd);
	
	current->pwd = inoptr;
	inoptr->i_count++;
	
	iput(inoptr);
	return 0;
}


int sys_chroot(const char * pathname)
{
	int ret;
	char * tmp;

	if(cklimit(pathname))
		return -EFAULT;

	sys_checkname(pathname);

	tmp = getname(pathname);
	if(!tmp)
		return -EAGAIN;

	ret = do_chroot(tmp);

	freename(tmp);
	
	return ret;
}

int sys_mkdir(const char * pathname, mode_t mode)
{
	int ret;
	char * tmp;

	if(cklimit(pathname))
		return -EFAULT;

	sys_checkname(pathname);

	tmp = getname(pathname);
	if(!tmp)
		return -EAGAIN;

	ret = do_mkdir(tmp, mode);

	freename(tmp);

	return ret;
}

int sys_rmdir(const char * pathname)
{
	int ret;
	char * tmp;

	if(cklimit(pathname))
		return -EFAULT;

	sys_checkname(pathname);

	tmp = getname(pathname);
	if(!tmp)
		return -EAGAIN;

	ret = do_rmdir(tmp);

	freename(tmp);

	return ret;
}

int sys_sync(void)
{
	bsync();

	return 0;
}

int sys_pipe(int filedes[2])
{
	int ret;

	if(cklimit(filedes) || ckoverflow(filedes, 2 * sizeof(int)))
		return -EFAULT;

	ret = do_pipe(filedes);

	return ret;
}

int sys_fcntl(int fd, int request, void * data)
{
	int ret = 0, fd1;
	unsigned int flag;
	struct filp * fp;
	struct inode * inoptr;

	if(cklimit(data))
		return -EFAULT;

	if(ckfdlimit(fd))
		return -EINVAL;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;

	inoptr->i_count++;

	switch(request){
	case F_DUPFD:
		fd1 = (int)data;

		ret = do_dup0(fd1, fd);
		if(ret >= 0)
			current->fflags[ret] &= ~O_CLOEXEC;

		break;
	case F_GETFD:
		ret = (current->fflags[fd] & O_CLOEXEC) ? 1 : 0;
		break;
	case F_SETFD:
		flag = (int)data;
		current->fflags[fd] |= flag ? O_CLOEXEC : 0;
		break;
	case F_GETFL: /* not support, always ok */
		break;
	case F_SETFL:
		break;
	case F_GETOWN:
		break;
	case F_SETOWN:
		break;
	default:
		ret = -ENOTSUP;
		break;
	}

	iput(inoptr);

	return ret;
}

int sys_ioctl(int fd, int request, void * data)
{
	struct filp * fp;
	struct inode * inoptr;
	int major, minor, error = 0;

	if(cklimit(data))
		return -EFAULT;

	if(ckfdlimit(fd))
		return -EINVAL;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	if(request == FIONBIO)
	{
		char no_block;

		if(ckoverflow(data, sizeof(char)))
			return -EFAULT;

		no_block = *(char *)data;

		//printk("blocked = %d\n", no_block);
		if (no_block)
		{
			current->fflags[fd] |= O_NDELAY;
		}
		else
		{
			current->fflags[fd] &= ~O_NDELAY;
		}
		
		return 0;
	}

	inoptr = fp->f_ino;

	if(!S_ISBLK(inoptr->i_mode) && !S_ISCHR(inoptr->i_mode) && \
		!S_ISSOCK(inoptr->i_mode))
		return -ENOTTY;

	// network interface set & get
	if (S_ISSOCK(inoptr->i_mode))
	{
		ilock(inoptr);
		inoptr->i_count++;

		if (request >= SIOCADDRT && request <= SIOCSIFVLAN)
		{
			error = do_with_netif_ioctl(request, data);
		}
		else
		{
			error = -ENOTSUP;
			do_syslog(
				LOG_KERN,
				"%s(%d) not support ioctl %x\n",
				current->myname,
				current->pid,
				request
				);
		}
		
		iput(inoptr);
		return error;
	}

	major = MAJOR(inoptr->i_zone[0]);
	minor = MINOR(inoptr->i_zone[0]);
	if(major >= get_dev_nr() || !dev_table[major].ioctl)
		return -ENODEV;

	ilock(inoptr);
	inoptr->i_count++;

	error = dev_table[major].ioctl(minor, request, data, fp->f_mode);

	iput(inoptr);

	return error;
}

int sys_access(const char * pathname, int expect)
{
	char * tmp;
	int error;

	if(expect > 7) /* RWX all set is 7, invalid value */
		return -EINVAL;

	if(cklimit(pathname))
		return -EFAULT;

	sys_checkname(pathname);

	tmp = getname(pathname);
	if(!tmp)
		return -EAGAIN;

	error = do_access(tmp, expect);

	freename(tmp);

	return error;
}

int sys_getcwd(char * buff, size_t size)
{
	if(size < 0)
		return -EINVAL;

	if(!size)
		return -ERANGE;

	if(cklimit(buff) || ckoverflow(buff, size))
		return -EFAULT;

	return do_getcwd(buff, size);
}

int sys_fsync(int fd)
{
	if(ckfdlimit(fd))
		return -EINVAL;

	if(!fget(fd))
		return -EBADF;

	bsync(); // XXX

	return 0;
}

int sys_chmod(const char * path, mode_t mode)
{
	int error;
	char * tmp;

	if(cklimit(path))
		return -EFAULT;

	sys_checkname(path);

	tmp = getname(path);
	if(!tmp)
		return -EAGAIN;

	error = do_chmod(tmp, mode);

	freename(tmp);

	return error;
}

int sys_fchmod(int fd, mode_t mode)
{
	struct filp *fp;
	struct inode *inoptr;
	
	if(ckfdlimit(fd))
		return -EINVAL;

	fp = fget(fd);
	if (!fp)
		return -EBADF;

	inoptr = fp->f_ino;
	ilock(inoptr);
	inoptr->i_count++;
	
	if ((current->euid != SU_UID) && (current->euid != inoptr->i_uid))
	{
		iput(inoptr);
		return -EACCES;
	}

	inoptr->i_dirty = 1;

	inoptr->i_mode |= mode & I_UGRWX;	
	inoptr->i_update |= CTIME;
	
	iput(inoptr);
	
	return 0;
}

int sys_chown(const char * path, uid_t owner, gid_t group)
{
	int error;
	char * tmp;

	if(cklimit(path))
		return -EFAULT;

	sys_checkname(path);

	tmp = getname(path);
	if(!tmp)
		return -EAGAIN;

	error = do_chown(tmp, owner, group);

	freename(tmp);

	return error;
}

int sys_fchown(int fd, uid_t owner, gid_t group)
{
	struct filp *fp;
	struct inode *inoptr;
	
	if(ckfdlimit(fd))
		return -EINVAL;

	fp = fget(fd);
	if (!fp)
		return -EBADF;

	inoptr = fp->f_ino;
	ilock(inoptr);
	inoptr->i_count++;

	if (current->euid == SU_UID)
		inoptr->i_uid = owner;
	else if ((current->euid != inoptr->i_uid) && (inoptr->i_uid != owner))
	{
		iput(inoptr);
		return -EACCES;
	}
	else if (current->egid != group)
	{
		iput(inoptr);
		return -EACCES;
	}

	inoptr->i_dirty = 1;
	inoptr->i_gid = group;
	inoptr->i_update |= CTIME;

	iput(inoptr);

	return 0;
}

int sys_utime(const char *path, const struct utimbuf *buf)
{
	int error;
	char *tmp;
	
	if (cklimit(path))
		return -EFAULT;

	sys_checkname(path);

	if (cklimit(buf))
	{
		return -EFAULT;
	}

	tmp = getname(path);
	if(!tmp)
		return -EAGAIN;

	error = do_utime(tmp, buf);

	freename(tmp);

	return error;
}

struct dirent {
	long		d_ino;
	long		d_off;
	unsigned short	d_reclen;
	char		d_name[256];
};

#define nameoff ((int)&(((struct dirent *)0)->d_name))

int sys_getdents(int fd, char * data, int count)
{
	struct filp * fp;
	struct inode * inoptr;
	int loc, ret, len;
	struct direct d;
	struct dirent * dirp;

	if(ckfdlimit(fd))
		return -EINVAL;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;
	if(!S_ISDIR(inoptr->i_mode))
		return -ENOTDIR;

	if(count < 0)
		return -EINVAL;

	if(cklimit(data) || ckoverflow(data, count))
		return -EFAULT;

	if(!count)
		return 0;

	loc = 0;

	nochk();

	do{
		if((count - loc) < nameoff + NAME_MAX + 1)
			break;

		ret = do_read(fd, (char *)&d, DIRECT_SIZE);
		if(ret < 0){
			rechk();
			return ret;
		}

		if(ret != DIRECT_SIZE){
			if(!ret) // end of dir
				break;

			rechk();
			return -EIO;
		}

		if(!d.d_ino)
			continue;

		for(len = 0; len < NAME_MAX; len++)
			if(!d.d_name[len])
				break;

		dirp = (struct dirent *)&data[loc];

		dirp->d_ino = d.d_ino;
		dirp->d_off = fp->f_pos; // not used, for linux
		dirp->d_reclen = ((nameoff + len + 1) + 3) & (~3);

		memcpy(dirp->d_name, d.d_name, len);
		dirp->d_name[len] = '\0';

		loc += dirp->d_reclen;
	}while(1);

	rechk();

	return loc;
}

static int get_fs_dev(const char *path, dev_t *pdev)
{
	int error;
	char * tmp;
	dev_t dev = NODEV;
	struct inode *inoptr;

	if (cklimit(path))
	{
		return -EFAULT;
	}

	sys_checkname(path);

	tmp = getname(path);
	if (!tmp)
	{
		return -EAGAIN;
	}

	sys_checkname(tmp);

	inoptr = namei(tmp, &error, 0);
	if (!inoptr)
	{
		freename(tmp);
		return error;
	}
	else
	{
		dev = inoptr->i_dev;
	}
	
	iput(inoptr);
	freename(tmp);

	*pdev = dev;
	
	return 0;
}

int sys_statfs(const char *path, struct statfs *stat)
{
	dev_t dev = NODEV;
	int error = 0;
	int count;
	struct super_block *sb;

	if (cklimit(path) || cklimit(stat) || !stat || \
		ckoverflow(stat, sizeof(struct statfs)))
	{
		return -EFAULT;
	}
	
	if ((error = get_fs_dev(path, &dev)) < 0)
	{
		return error;
	}

	if (!(sb = get_sb(dev)))
	{
		return -ENODEV;
	}

	memset(stat, 0, sizeof(struct statfs));

	stat->f_type = sb->s_magic;
	stat->f_bsize = BLOCKSIZ;
	stat->f_blocks = sb->s_zones - sb->s_fdzone;
	if ((error = get_free_bits_nr(sb, &count, ZMAP)) < 0)
	{
		return error;
	}

	stat->f_bfree = count;
	stat->f_bavail = count;
	stat->f_files = sb->s_ninodes;
	if ((error = get_free_bits_nr(sb, &count, IMAP)) < 0)
	{
		return error;
	}
	stat->f_ffree = count;
	stat->f_namelen = NAME_MAX;
	
	return error;
}

int sys_fstatfs(int fd, struct statfs *stat)
{
	dev_t dev = NODEV;
	int error = 0;
	int count;
	struct super_block *sb;
	struct filp *fp;
	struct inode *inoptr;

	if (cklimit(stat) || !stat || \
		ckoverflow(stat, sizeof(struct statfs)))
	{
		return -EFAULT;
	}
	
	if(ckfdlimit(fd))
		return -EINVAL;
	
	fp = fget(fd);
	if (!fp)
	{
		return -EBADF;
	}

	inoptr = fp->f_ino;
	ilock(inoptr);
	inoptr->i_count++;
	dev = inoptr->i_dev;
	iput(inoptr);

	if (!(sb = get_sb(dev)))
	{
		return -ENODEV;
	}

	memset(stat, 0, sizeof(struct statfs));

	stat->f_type = sb->s_magic;
	stat->f_bsize = BLOCKSIZ;
	stat->f_blocks = sb->s_zones - sb->s_fdzone;
	if ((error = get_free_bits_nr(sb, &count, ZMAP)) < 0)
	{
		return error;
	}

	stat->f_bfree = count;
	stat->f_bavail = count;
	stat->f_files = sb->s_ninodes;
	if ((error = get_free_bits_nr(sb, &count, IMAP)) < 0)
	{
		return error;
	}
	stat->f_ffree = count;
	stat->f_namelen = NAME_MAX;
	
	return error;
}
