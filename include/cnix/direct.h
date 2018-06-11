#ifndef _DIRECT_H
#define _DIRECT_H

#include <cnix/limits.h>
#include <cnix/inode.h>

struct direct{
	ino_t d_ino;
	char d_name[NAME_MAX];
};

#define DIRECT_SIZE	sizeof(struct direct)

#endif
