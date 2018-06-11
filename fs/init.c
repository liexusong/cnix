#include <cnix/fs.h>
#include <cnix/kernel.h>

void fs_init(void)
{
	init_super();
	init_inode();
	init_filp();
}
