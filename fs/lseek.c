#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/devices.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>

off_t do_lseek(int fd, off_t offset, int whence)
{
	off_t pos;
	struct filp * fp;
	struct inode * inoptr;

	fp = fget(fd);
	if(!fp)
		DIE("BUG: cannot happen\n");

	inoptr = fp->f_ino;
	if(inoptr->i_flags & PIPE_I)
		return -ESPIPE;

	switch(whence){
	case SEEK_SET:
		pos = 0;
		break;
	case SEEK_CUR:
		pos = fp->f_pos;
		break;
	case SEEK_END:
		pos = inoptr->i_size;
		break;
	default:
		return -EINVAL;	
	}

	if((offset < 0) && ((pos + offset) > pos))
		return -EINVAL;

	if((offset > 0) && ((pos + offset) < pos))
		return -EINVAL;
	
	pos += offset;

	/* read ahead */
	if(pos != fp->f_pos)
		inoptr->i_flags |= SEEK_I;

	fp->f_pos = pos;

	return pos;
}
