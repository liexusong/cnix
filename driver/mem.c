#include <cnix/errno.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/driver.h>
#include <cnix/fs.h>
#include <cnix/devices.h>

int mem_open(dev_t dev, int flags)
{
	switch(dev){
	case MEM_DEV:
		if(current->euid != SU_UID)
			return -EACCES;
		break;
	case NULL_DEV:
	case ZERO_DEV:
		break;
	default:
		return -ENXIO;
	}

	return 0;
}

int mem_close(dev_t dev, int flags)
{
	if((dev != MEM_DEV) && (dev != NULL_DEV) && (dev != ZERO_DEV))
		return -ENXIO;

	return 0;
}

ssize_t mem_read(
	dev_t dev,
	char * buffer,
	size_t count,
	off_t off,
	int * error,
	int openflags
	)
{
	int ret = count;

	switch(dev){
	case MEM_DEV:
		memcpy(buffer, (void *)off, count);
		break;
	case NULL_DEV:
		ret = 0;
		break;
	case ZERO_DEV:
		memset(buffer, 0, count);
		break;
	default:
		*error = -ENXIO;
		ret = 0;
		break;
	}

	return ret;
}

ssize_t mem_write(
	dev_t dev,
	const char * buffer,
	size_t count,
	off_t off,
	int * error,
	int openflags
	)
{
	int ret = count;

	switch(dev){
	case MEM_DEV:
		memcpy((void *)off, buffer, count);
		break;
	case NULL_DEV:
		break;
	case ZERO_DEV:
		break;
	default:
		*error = -ENXIO;
		ret = 0;
		break;
	}

	return ret;
}

/*
 * caller has locked inode
 */
int mem_ioctl(dev_t dev, int request, void * data, int openflags)
{
	PRINTK("mem_ioctl\n");
	return 0;
}

void mem_driver_init(void)
{
	printk("Mem driver initializing\n");
}
