#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/devices.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>
#include <asm/system.h>

static ssize_t dev_read(
	struct inode * inoptr,
	char * buffer,
	size_t count, 
	off_t pos,
	int * error,
	int flags
	)
{
	dev_t dev;
	ssize_t readed;

	readed = 0;
	dev = (dev_t)inoptr->i_zone[0];
	
	if(MAJOR(dev) < get_dev_nr() && dev_table[MAJOR(dev)].read)
		readed = dev_table[MAJOR(dev)].read(
			MINOR(dev),
			buffer,
			count,
			pos,
			error,
			flags
			);
	
	return readed;			
}

extern ssize_t socket_read(
	struct inode * inoptr,
	char * buffer,
	size_t count,
	int * error,
	int flags
	);

/*
 * need to lock inoptr before any operation can cause sleeping?
 * the same question for write.
 */
ssize_t do_read(int fd, char * buffer, size_t count)
{
	int error = 0;
	off_t pos, inblock;
	block_t block;
	ssize_t readed, reading;
	struct filp * fp;
	struct inode * inoptr;
	struct buf_head * bh;
	unsigned long flags;

	fp = fget(fd);
	if(!fp)
		DIE("BUG: cannot happen\n");

	if(count < 0)
		DIE("BUG: cannot happen\n");

	if(!count)
		return 0;

	readed = 0;

	inoptr = fp->f_ino;

	if(S_ISBLK(inoptr->i_mode) || S_ISCHR(inoptr->i_mode)){
		readed = dev_read(
			inoptr,
			buffer,
			count,
			fp->f_pos,
			&error,
			fp->f_mode | current->fflags[fd]
			);
		if(error < 0)
			goto errout;

		goto out;
	}else if(S_ISSOCK(inoptr->i_mode)){
		readed = socket_read(
			inoptr,
			buffer,
			count,
			&error,
			fp->f_mode | current->fflags[fd]
		);
		if(error < 0)
			return error;

		return readed;
	}else if(S_ISDIR(inoptr->i_mode) && current->check){
		error = -EISDIR;
		goto errout;
	}else if(S_ISFIFO(inoptr->i_mode)){
		error = -EINVAL;
		goto errout;
	}

	if(inoptr->i_flags & PIPE_I){
		int readp, count1;
		unsigned char * ibuffer;

		count1 = count;

check_again:
		if(!inoptr->i_size){
			if(current->fflags[fd] & O_NDELAY)
				return readed;

			if(inoptr->i_buddy){
				lockb_all(flags);

				current->sleep_spot = &inoptr->i_wait;
				sleepon(&inoptr->i_wait);

				if(anysig(current)){
					unlockb_all(flags);
					return -EINTR;
				}
				unlockb_all(flags);

				goto check_again;
			}

			return readed;
		}

		if(inoptr->i_size < count)
			count = inoptr->i_size;

		readp = inoptr->i_readp;
		ibuffer = inoptr->i_buffer;

		while(count > 0){
			reading = 4096 - readp;
			if(reading > count)
				reading = count;

			memcpy(&buffer[readed], &ibuffer[readp], reading);
			
			readp += reading;
			if(readp >= 4096)
				readp = 0;

			inoptr->i_readp = readp;
			inoptr->i_size -= reading;

			readed += reading;
			count -= reading;
		}

		wakeup(&inoptr->i_wait);
		select_wakeup(&inoptr->i_select,  OPTYPE_WRITE);

		if(readed < count1){
			count = count1 - readed;
			goto check_again;
		}

		return readed;
	}

	pos = fp->f_pos;

	inblock = pos % BLOCKSIZ;
	if(inblock){
		reading = BLOCKSIZ - inblock;
		if(reading > count)
			reading = count;

		if(reading > inoptr->i_size - pos)
			reading = inoptr->i_size - pos;

		if(reading <= 0)
			goto out;

		block = bmap(inoptr, pos);
		if(block == NOBLOCK){
			memset(&buffer[readed], 0, reading);
		}else{
			bh = bread(inoptr->i_dev, block);
			if(!bh){
				error = -EIO;
				goto errout;
			}

			/* how if execess buffer limit? */
			memcpy(&buffer[readed], &bh->b_data[inblock], reading);

			brelse(bh);
		}

		readed += reading;
		pos += reading;
	}

	while(count > readed){
		if((count - readed) > BLOCKSIZ)
			reading = BLOCKSIZ;
		else
			reading = count - readed;

		if(reading > inoptr->i_size - pos)
			reading = inoptr->i_size -  pos;

		if(reading <= 0)
			break;

		block = bmap(inoptr, pos);
		if(block == NOBLOCK)
			memset(&buffer[readed], 0, reading);
		else{
			/* signal EINTR? */
			bh = bread(inoptr->i_dev, block);
			if(!bh){
				error = -EIO;
				goto errout;
			}

			/* how if execess buffer limit? */
			memcpy(&buffer[readed], bh->b_data, reading);

			brelse(bh);
		}

		readed += reading;
		pos += reading;
	}

out:
	fp->f_pos += readed;
	return readed;

errout:
	fp->f_pos += readed;
	return error;
}
