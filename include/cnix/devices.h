#ifndef _DEVICES_H
#define _DEVICES_H

#include <cnix/types.h>

#define MEM_DEV 1
#define NULL_DEV 3
#define ZERO_DEV 5

struct dev_struct{
	int (*open)(dev_t, int);
	ssize_t (*read)(dev_t, char *, size_t, off_t, int *, int);
	ssize_t (*write)(dev_t, const char *, size_t, off_t, int *, int);
	int (*close)(dev_t, int);
	int (*ioctl)(dev_t, int, void *, int);
};

extern struct dev_struct dev_table[];
extern int get_dev_nr(void);

#endif
