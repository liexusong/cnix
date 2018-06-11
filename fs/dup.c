#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/devices.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>

int do_dup0(int startfd, int fd)
{
	int i, j;
	struct filp * fp;

	if(ckfdlimit(startfd))
		return -EINVAL;

	if(ckfdlimit(fd))
		return -EINVAL;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	/* fuse(fp), but doesn't need to increment fp->f_ino->i_count */
	for(i = 0, j = startfd; i < OPEN_MAX; i++, j++){
		j %= OPEN_MAX;

		if(!current->file[j]){
			current->file[j] = fp;
			fuse(fp);
			current->fflags[j] = current->fflags[fd];

			return j;
		}
	}

	return -EMFILE;
}

int do_dup(int fd)
{
	if(ckfdlimit(fd))
		return -EINVAL;

	return do_dup0(0, fd);
}

int do_dup2(int oldfd, int newfd)
{
	struct filp * fp, * fp1;

	if(ckfdlimit(oldfd))
		return -EINVAL;

	if(ckfdlimit(newfd))
		return -EINVAL;

	if(oldfd == newfd)
		return newfd;

	fp = fget(oldfd);
	if(!fp)
		return -EBADF;

	fp1 = fget(newfd);
	if(fp1)
		do_close(newfd);

	current->fflags[newfd] = current->fflags[oldfd];

	current->file[newfd] = fp;
	fuse(fp);

	return newfd;
}
