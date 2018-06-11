#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>

int do_mount(const char * specfile, const char * dirname, int flag)
{
	int error;
	dev_t dev;
	struct super_block * tmp;
	struct inode * inoptr, * dirino, * rooti;

	checkname(specfile);
	checkname(dirname);

	if(current->euid != SU_UID)
		return -EPERM;

	inoptr = namei(specfile, &error, 0);
	if(!inoptr)
		return error;

	if(!S_ISBLK(inoptr->i_mode)){
		iput(inoptr);
		return -EPERM;
	}

	dirino = namei(dirname, &error, 0);
	if(!dirino){
		iput(inoptr);
		return error;
	}

	if(!S_ISDIR(dirino->i_mode)){
		iput(dirino);
		iput(inoptr);
		return -ENOTDIR;
	}

	if(dirino->i_flags & MOUNT_I){
		iput(dirino);
		iput(inoptr);
		return -EBUSY;
	}

	dev = (dev_t)inoptr->i_zone[0];

	tmp = sread(dev);

	/* to avoid dead lock when mount(?, "/", ?); */
	if((dev == dirino->i_dev) && (dirino->i_num == ROOT_INODE)){
		rooti = dirino;
		rooti->i_count++;
	}else
		rooti = iget(dev, ROOT_INODE); 

	if(!rooti){
		iput(dirino);
		iput(inoptr);
		return -EPERM;
	}

	tmp->s_rooti = rooti;
	rooti->i_count++;
	iput(rooti);

	tmp->s_mounti = dirino;

	dirino->i_mounts = tmp;
	dirino->i_flags |= MOUNT_I;
	dirino->i_count++;
	iput(dirino);

	tmp->s_roflag = ((flag & O_ACCMODE) == O_RDONLY) ? 1 : 0;

	iput(inoptr);

	return 0;
}

int do_umount(const char * specfile)
{
	int error;
	dev_t dev;
	struct super_block * tmp;
	struct inode * inoptr, * rooti, * dirino;

	checkname(specfile);

	if(current->euid != SU_UID)
		return - EPERM;

	inoptr = namei(specfile, &error, 0);
	if(!inoptr)
		return error;

	if(!S_ISBLK(inoptr->i_mode)){
		iput(inoptr);
		return -EPERM;
	}

	dev = (dev_t)inoptr->i_zone[0];
	if(get_iiucount(dev) > 1){
		iput(inoptr);
		return -EBUSY;
	}
	
	tmp = sread(dev);

	rooti = tmp->s_rooti;
	iput(rooti);

	dirino = tmp->s_mounti;
	dirino->i_flags &= ~MOUNT_I;
	iput(dirino);

	/* call device function to do something useful? */
	
	bsync();
	inval_dev(dev);
	
	sfree(tmp);

	return 0;
}
