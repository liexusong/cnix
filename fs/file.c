#include <cnix/errno.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>
#include <cnix/devices.h>

static struct filp filps[NR_FILP];
int from_filp;

void init_filp(void)
{
	int i;

	for(i = 0; i < NR_FILP; i++)
		filps[i].f_count = 0; /* free */

	from_filp = 0;
}

static struct filp * get_filp(void)
{
	int i;

	i = from_filp;
	do{
		if(filps[i].f_count == 0){
			filps[i].f_count++;

			from_filp = (i + 1) % NR_FILP;

			return &filps[i];	
		}

		i = (i + 1) % NR_FILP;
	}while(i != from_filp);

	return NULL;
}

struct filp * fget(int fd)
{
	if(ckfdlimit(fd))
		DIE("cannot happen\n");

	return current->file[fd];
}

/*
 * will be called in close
 */
void ffree(struct filp * fp)
{
	int major, minor;
	struct inode * inoptr;

	--fp->f_count;
	if(fp->f_count == 0){
		inoptr = fp->f_ino;

		if(!inoptr)
			DIE("cannot happen\n");

		iput(inoptr);

		/* block device or char device */
		if((S_ISBLK(inoptr->i_mode) || S_ISCHR(inoptr->i_mode))){
			major = MAJOR(inoptr->i_zone[0]);
			minor = MINOR(inoptr->i_zone[0]);

			if(major >= get_dev_nr() || !dev_table[major].close)
				DIE("cannot happen\n");

			dev_table[major].close(minor, fp->f_mode);
		}
	}
}

/*
 * will be called in fork, dup, dup2
 */
void fuse(struct filp * fp)
{
	fp->f_count++;
}

/*
 * start for F_DUPFD
 */
int fdget(int start, mode_t mode, struct inode * inoptr, int * error)
{
	int i;
	struct filp * tmp;

	for(i = start; i < OPEN_MAX; i++)
		if(!current->file[i]){
			tmp = get_filp();
			if(!tmp){
				*error = -ENOMEM;
				return -1;
			}

			tmp->f_mode = mode;
			tmp->f_count = 1;
			tmp->f_ino = inoptr;
			tmp->f_pos = 0;

			current->file[i] = tmp;
			current->fflags[i] = O_CLOEXEC;

			return i;
		}

	*error = -ENFILE;

	return -1;
}
