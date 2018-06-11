#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/devices.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>

int do_access(const char * pathname, int expect)
{
	mode_t mode;
	uid_t uid;
	gid_t gid;
	int error;
	struct inode * inoptr;

	checkname(pathname);

	inoptr = namei(pathname, &error, 0);
	if(!inoptr)
		return error;

	error = 0;

	if(expect == F_OK) /* file exists */
		goto out;

	mode = inoptr->i_mode;
	uid = current->uid;
	gid = current->gid;

	if(uid == SU_UID){ /* super user */
		/* expect executable, but not a executable file */
		if((expect & 1) && !(mode & (1 + 8 + 64)))
			error = -EACCES;

		goto out;
	}

	/* owner */
	if((uid == inoptr->i_uid) && ((expect << 6) & mode))
		goto out;

	/* same group */
	if((gid == inoptr->i_gid) && ((expect << 3) & mode))
		goto out;

	/* other user */
	if(expect & mode)
		goto out;

	error = -EACCES;

out:
	iput(inoptr);

	return error;
}

int do_chmod(const char * path, mode_t mode)
{
	int error;
	struct inode * inoptr;

	checkname(path);

	inoptr = namei(path, &error, 0);
	if(!inoptr)
		return error;

	if((current->euid != SU_UID) && (current->euid != inoptr->i_uid)){
		iput(inoptr);
		return -EACCES;
	}

	inoptr->i_dirty = 1;

	inoptr->i_mode |= mode & I_UGRWX;	
	inoptr->i_update |= CTIME;

	iput(inoptr);

	return 0;
}

int do_chown(const char * path, uid_t owner, gid_t group)
{
	int error;
	struct inode * inoptr;

	checkname(path);

	inoptr = namei(path, &error, 0);
	if(!inoptr)
		return error;

	if(current->euid == SU_UID)
		inoptr->i_uid = owner;
	else if((current->euid != inoptr->i_uid) && (inoptr->i_uid != owner)){
		iput(inoptr);
		return -EACCES;
	}else if(current->egid != group){
		iput(inoptr);
		return -EACCES;
	}

	inoptr->i_dirty = 1;

	if(group > 0)
		inoptr->i_gid = group;

	inoptr->i_update |= CTIME;

	iput(inoptr);

	return error;
}

static BOOLEAN can_write(struct inode *inoptr)
{
	uid_t uid = current->uid;
	gid_t gid = current->gid;
	mode_t mode;

	mode = inoptr->i_mode;
	
	/* owner */
	if ((uid == inoptr->i_uid) && ((I_WB << 6) & mode))
		return TRUE;

	/* same group */
	if ((gid == inoptr->i_gid) && ((I_WB << 3) & mode))
		return TRUE;

	/* other user */
	if (I_WB & mode)
		return TRUE;

	return FALSE;
}

int do_utime(const char *path, const struct utimbuf *buf)
{
	int error = -EACCES;
	struct inode * inoptr;

	checkname(path);

	inoptr = namei(path, &error, 0);
	if(!inoptr)
		return error;

	if (current->euid != SU_UID && current->euid != inoptr->i_uid)
	{
		if (buf || !can_write(inoptr))
		{
			iput(inoptr);
			return -EACCES;
		}
	}

	error = 0;
	
	if (buf)
	{
		inoptr->i_atime = buf->actime;
		inoptr->i_mtime = buf->modtime;
	}
	else
	{
		inoptr->i_update |= (ATIME | MTIME);
	}
	
	inoptr->i_update |= CTIME;
	inoptr->i_dirty = 1;

	iput(inoptr);

	return error;
}
