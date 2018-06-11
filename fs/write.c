#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/devices.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>
#include <asm/system.h>

static ssize_t dev_write(
	struct inode * inoptr,
	const char * buffer,
	size_t count,
	off_t pos,
	int * error,
	int flags
	)
{
	dev_t dev;
	ssize_t writed;
	
	writed = 0;
	dev = (dev_t)inoptr->i_zone[0];

	if(MAJOR(dev) < get_dev_nr() && dev_table[MAJOR(dev)].write)
		writed = dev_table[MAJOR(dev)].write(
				MINOR(dev),
				buffer,
				count,
				pos,
				error,
				flags
				);

	return writed;
}

extern ssize_t socket_write(
	struct inode * inoptr,
	const char * buffer,
	size_t count,
	int * error,
	int falgs
	);

ssize_t do_write(int fd, const char * buffer, size_t count)
{
	int error = 0;
	off_t pos, inblock;
	block_t block;
	ssize_t writed, writing;
	struct filp * fp;
	struct inode * inoptr;
	struct buf_head * bh;
	unsigned long flags;

	fp = fget(fd);
	if(!fp)
		DIE("BUG: cannot happen\n");

	if(count < 0)
		DIE("BUG: cannot happen\n");
	
	if((fp->f_mode & O_ACCMODE) == O_RDONLY)
		return -EINVAL;

	if(S_ISDIR(fp->f_ino->i_mode))
		return -EISDIR;

	if(!count)
		return 0;

	inoptr = fp->f_ino;

	if(S_ISBLK(inoptr->i_mode) || S_ISCHR(inoptr->i_mode)){
		writed = dev_write(
			inoptr,
			buffer,
			count,
			fp->f_pos,
			&error,
			fp->f_mode | current->fflags[fd]
			);

		fp->f_pos += writed;

		if(error < 0)
			return error;

		return writed;
	}else if(S_ISSOCK(inoptr->i_mode)){
		writed = socket_write(
			inoptr,
			buffer,
			count,
			&error,
			fp->f_mode | current->fflags[fd]
			);
		if(error < 0)
			return error;

		return writed;
	}else if(S_ISFIFO(inoptr->i_mode)){
		inoptr->i_count++;
		iput(inoptr);
		return -EINVAL;
	}

	if(inoptr->i_flags & PIPE_I){
		int writep, count1;
		unsigned char * ibuffer;

		writed = 0;
		count1 = count;

check_again:
		if(!inoptr->i_buddy){
			sendsig(current, SIGPIPE);
			return -EPIPE;
		}

		if(inoptr->i_size >= 4096){
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

		if(count > 4096 - inoptr->i_size)
			count = 4096 - inoptr->i_size;

		writep = inoptr->i_writep;
		ibuffer = inoptr->i_buffer;

		while(count > 0){
			writing = 4096 - writep;
			if(writing > count)
				writing = count;

			memcpy(&ibuffer[writep], &buffer[writed], writing);

			writep += writing;
			if(writep >= 4096)
				writep = 0;

			inoptr->i_writep = writep;
			inoptr->i_size += writing;

			writed += writing;
			count -= writing;
		}

		wakeup(&inoptr->i_wait);
		select_wakeup(&inoptr->i_select, OPTYPE_READ);

		if(writed < count1){
			count = count1 - writed;
			goto check_again;
		}

		return writed;
	}

	writed = 0;
	pos = fp->f_pos;
	
	inblock = pos % BLOCKSIZ;
	if(inblock){
		if(pos >= FILESIZE_MAX)
		       return -EFBIG;

		writing = BLOCKSIZ - inblock;
		if(writing > count)
			writing = count;

		block = bmap(inoptr, pos);
		if(block == NOBLOCK){
			/* XXX: error if block size is not equal to zone */
			block = (block_t)bitalloc(inoptr->i_sp, ZMAP);
			if(block == NOBLOCK){
				printk("out of disk space\n");
				goto errout;
			}

			bh = getblk(inoptr->i_dev, block);
			if(!bh){
				bitfree(inoptr->i_sp, (bit_t)block, ZMAP);
				goto errout;
			}
			
			memset(bh->b_data, 0, BLOCKSIZ);
			if(!minode(inoptr, pos, 
				(zone_t)(block >> inoptr->i_sp->s_lzsize))){
				bitfree(inoptr->i_sp, (bit_t)block, ZMAP);
				brelse(bh);
				goto errout;
			}
		}else{
			bh = bread(inoptr->i_dev, block);
			if(!bh)
				goto errout;
		}

		/* XXX: check buffer limit */
		memcpy(&bh->b_data[inblock], buffer, writing);

		if(fp->f_mode & O_SYNC){ /* XXX: need test */
			bwrite(bh);
			brelse(bh);
		}else{
			bh->b_flags |= B_DIRTY | B_DONE; /* delay write */
			brelse(bh);
		}

		writed += writing;

		pos += writing;
		if(pos > inoptr->i_size)
			inoptr->i_size = pos;
	}

	while(writed < count){
		if(pos >= FILESIZE_MAX)
			return -EFBIG;

		if((count - writed) > BLOCKSIZ)
			writing = BLOCKSIZ;
		else
			writing = count - writed;

		block = bmap(inoptr, pos);
		if(block == NOBLOCK){
			/* XXX: error if block size is not equal to zone */
			block = (block_t)bitalloc(inoptr->i_sp, ZMAP);
			if(block == NOBLOCK){
				printk("out of disk space\n");
				goto errout;
			}

			bh = getblk(inoptr->i_dev, block);
			if(!bh){
				bitfree(inoptr->i_sp, (bit_t)block, ZMAP);
				goto errout;
			}

			memset(bh->b_data, 0, BLOCKSIZ);
			if(!minode(inoptr, pos, 
				(zone_t)(block >> inoptr->i_sp->s_lzsize))){
				bitfree(inoptr->i_sp, (bit_t)block, ZMAP);
				brelse(bh);
				goto errout;
			}
		}else{
			bh = bread(inoptr->i_dev, block);
			if(!bh)
				goto errout;
		}

		/* XXX: check buffer limit */
		memcpy(bh->b_data, &buffer[writed], writing);

		if(fp->f_mode & O_SYNC){ /* XXX: need test */
			bwrite(bh);
			brelse(bh);
		}else{
			bh->b_flags |= B_DIRTY | B_DONE; /* delay write */
			brelse(bh);
		}

		writed += writing;

		pos += writing;
		if(pos > inoptr->i_size)
			inoptr->i_size = pos;
	}

	fp->f_pos = pos;
	if(pos > inoptr->i_size)
		inoptr->i_size = pos;

	if (S_ISREG(inoptr->i_mode) || S_ISDIR(inoptr->i_mode))
	{
		inoptr->i_update |= MTIME;
		inoptr->i_dirty = 1;
	}
	
	inoptr->i_count++;
	iput(inoptr);

	return writed;

errout:
	fp->f_pos = pos;
	if(pos > inoptr->i_size)
		inoptr->i_size = pos;

	if (S_ISREG(inoptr->i_mode) || S_ISDIR(inoptr->i_mode))
	{
		inoptr->i_update |= MTIME;
		inoptr->i_dirty = 1;
	}

	inoptr->i_count++;
	iput(inoptr);

	return -EIO;
}
