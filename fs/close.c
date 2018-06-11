#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/mm.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>
#include <cnix/devices.h>

int do_close(int fd)
{
	struct filp * fp;

	fp = fget(fd);
	if(!fp)
		DIE("BUG: cannot happen\n");

	ffree(fp);

	current->file[fd] = NULL;

	return 0;
}
