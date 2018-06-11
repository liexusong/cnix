#include <cnix/errno.h>
#include <cnix/string.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>
#include <cnix/driver.h>

extern int ide_open(dev_t dev, int flags);
extern size_t ide_size(dev_t dev);

int hd_open(dev_t dev, int flags)
{
	if(ide_open(dev, flags) < 0)
		return -ENXIO;

	return 0;
}

int hd_close(dev_t dev, int flags)
{
	if(ide_open(dev, flags) < 0)
		return -ENXIO;

	return 0;
}

ssize_t hd_read(
	dev_t dev,
	char * buffer,
	size_t count,
	off_t off,
	int * error,
	int openflags
	)
{
	block_t block;
	off_t pos, inblock;
	int readed, reading;
	struct buf_head * bh;
	size_t size = ide_size(dev);

	readed = 0;
	pos = off;

	inblock = pos % BLOCKSIZ;
	if(inblock){
		reading = BLOCKSIZ - inblock;
		if(reading > count)
			reading = count;

		if(reading > size - pos)
			reading = size - pos;

		if(reading <= 0)
			return readed;

		block = pos / BLOCKSIZ;
		bh = bread(dev, block);
		if(!bh){
			*error = -EIO;
			return 0;
		}

		/* how if execess buffer limit? */
		memcpy(&buffer[readed], &bh->b_data[inblock], reading);

		brelse(bh);

		readed += reading;
		pos += reading;
	}

	while(count > readed){
		if((count - readed) > BLOCKSIZ)
			reading = BLOCKSIZ;
		else
			reading = count - readed;

		if(reading > size - pos)
			reading = size -  pos;

		if(reading <= 0)
			break;

		block = pos / BLOCKSIZ;

		/* signal EINTR? */
		bh = bread(dev, block);
		if(!bh){
			*error = -EIO;
			return 0;
		}

		/* how if execess buffer limit? */
		memcpy(&buffer[readed], bh->b_data, reading);

		brelse(bh);

		readed += reading;
		pos += reading;
	}

	return readed;
}

ssize_t hd_write(
	dev_t dev,
	const char * buffer,
	size_t count,
	off_t off,
	int * error,
	int openflags
	)
{
	block_t block;
	off_t pos, inblock;
	int writed, writing;
	struct buf_head * bh;
	size_t size = ide_size(dev);

	writed = 0;
	pos = off;
	
	inblock = pos % BLOCKSIZ;
	if(inblock){
		if(pos >= size){
		       *error = -EFBIG;
			return 0;
		}

		writing = BLOCKSIZ - inblock;
		if(writing > count)
			writing = count;

		block = pos / BLOCKSIZ;

		bh = bread(dev, block);
		if(!bh){
			*error = -EIO;
			return 0;
		}

		/* XXX: check buffer limit */
		memcpy(&bh->b_data[inblock], buffer, writing);

		if(openflags & O_SYNC){ /* XXX: need test */
			bwrite(bh);
			brelse(bh);
		}else{
			bh->b_flags |= B_DIRTY | B_DONE; /* delay write */
			brelse(bh);
		}

		writed += writing;
		pos += writing;
	}

	while(writed < count){
		if(pos >= size){
			*error = -EFBIG;
			return 0;
		}

		if((count - writed) > BLOCKSIZ)
			writing = BLOCKSIZ;
		else
			writing = count - writed;

		block = pos / BLOCKSIZ;

		bh = bread(dev, block);
		if(!bh){
			*error = -EIO;
			return 0;
		}

		/* XXX: check buffer limit */
		memcpy(bh->b_data, &buffer[writed], writing);

		if(openflags & O_SYNC){ /* XXX: need test */
			bwrite(bh);
			brelse(bh);
		}else{
			bh->b_flags |= B_DIRTY | B_DONE; /* delay write */
			brelse(bh);
		}

		writed += writing;

		pos += writing;
	}

	return writed;
}

/*
 * caller has locked inode
 */
int hd_ioctl(dev_t dev, int request, void * data, int openflags)
{
	return 0;
}

void hd_driver_init(void)
{
	printk("Harddisk driver initializing\n");
}
