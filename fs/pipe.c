#include <cnix/errno.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>
#include <cnix/mm.h>

int do_pipe(int filedes[2])
{
	int error;
	int fd0, fd1;
	struct filp * fp0, * fp1;
	struct inode * inoptr;
	unsigned char * buffer;

	buffer = (unsigned char *)get_one_page();
	if(!buffer)
		return -EAGAIN;

	/* won't fail */
	inoptr = iget(NODEV, 0);

	inoptr->i_mode = 0;
	inoptr->i_nlinks = 0;
	inoptr->i_size = 0;
	inoptr->i_buddy = 1;
	inoptr->i_buffer = NULL;
	inoptr->i_readp = inoptr->i_writep = 0;
	select_init(&inoptr->i_select);

	fd0 = fdget(0, O_RDONLY, inoptr, &error);
	if(fd0 < 0){
		free_one_page((unsigned long)buffer);
		iput(inoptr);
		return error;
	}

	fd1 = fdget(0, O_WRONLY, inoptr, &error);
	if(fd1 < 0){
		free_one_page((unsigned long)buffer);
		ffree(fget(fd0));
		current->file[fd0] = NULL;
		iput(inoptr);
		return error;
	}

	fp0 = fget(fd0);
	fp1 = fget(fd1);

	inoptr->i_buffer = buffer;
	inoptr->i_flags |= PIPE_I;

	inoptr->i_count += 2;
	iput(inoptr);

	filedes[0] = fd0;
	filedes[1] = fd1;

	return 0;
}
