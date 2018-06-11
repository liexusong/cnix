#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>

int do_mkdir(const char * pathname, mode_t mode)
{
	int error = 0;
	char part[NAME_MAX + 1];
	struct inode * fino, * inoptr;
	mode_t mode1;

	checkname(pathname);

	fino = namei(pathname, &error, 1);
	if(!fino)
		return error;

	if(!S_ISDIR(fino->i_mode)){
		iput(fino);
		return -ENOTDIR;
	}

	if(admit(fino, I_WB) == 0){
		iput(fino);
		return -EACCES;
	}

	if(!get_tail(pathname, part)){
		iput(fino);
		return -ENAMETOOLONG;
	}

	fino->i_count++;
	inoptr = ifind(fino, part);
	if(inoptr){
		iput(fino);
		iput(inoptr);
		return -EEXIST;
	}

	/* lock again, because has been iput in ifind */
	ilock(fino);

	mode1 = ((mode & I_UGRWX) & (~(current->umask))) | I_DIR;
	inoptr = iinsert(fino, part, mode1, &error, 0);

	if(!inoptr){
		iput(fino);
		return error;
	}	

	mode1 = ((mode & I_UGRWX) & (~(current->umask))) | I_DIR;
	iinsert(inoptr, ".", mode1, &error, inoptr->i_num);
	iinsert(inoptr, "..", mode1, &error, fino->i_num);

	inoptr->i_nlinks += 1; // ialloc will set nlinks to 1
	inoptr->i_dirty = 1;
	iput(inoptr);

	fino->i_nlinks++;
	fino->i_update |= MTIME;
	fino->i_dirty = 1;
	iput(fino);

	return 0;
}

int do_rmdir(const char * pathname)
{
	int error = 0;
	char part[NAME_MAX + 1];
	struct inode * fino, * inoptr;

	checkname(pathname);

	if(!get_tail(pathname, part))
		return -ENAMETOOLONG;

	if(!strcmp(part, ".") || !strcmp(part, ".."))
		return -EINVAL;

	fino = namei(pathname, &error, 1);
	if(!fino)
		return error;

	if(!S_ISDIR(fino->i_mode)){
		iput(fino);
		return -ENOTDIR;
	}

	if(!admit(fino, I_WB)){
		iput(fino);
		return -EACCES;
	}

	if(strlen(part) > NAME_MAX){
		iput(fino);
		return -ENAMETOOLONG;
	}

	fino->i_count++;
	inoptr = ifind(fino, part);
	if(!inoptr){
		iput(fino);
		return -ENOENT;
	}

	if((inoptr == current->pwd) || (inoptr == current->root)){
		iput(inoptr);
		iput(fino);
		return -EBUSY;
	}

	if(!S_ISDIR(inoptr->i_mode)){
		iput(inoptr);
		iput(fino);
		return -ENOTDIR;
	}

	if(!iempty(inoptr)){
		iput(inoptr);
		iput(fino);
		return -ENOTEMPTY;
	}

	if(inoptr->i_nlinks != 2)
		DIE("BUG: links(%d) not equal to 2\n", inoptr->i_nlinks);

	inoptr->i_nlinks -= 2; // inoptr->i_nlinks = 0;
	inoptr->i_dirty = 1;
	iput(inoptr);

	ilock(fino); /* lock again */
	if(!idelete(fino, part, &error)){
		iput(fino);
		return error;
	}

	fino->i_nlinks--;
	fino->i_update |= MTIME;
	fino->i_dirty = 1;
	iput(fino);

	return 0;
}

int do_chdir(const char * pathname)
{
	int error = 0;
	struct inode * inoptr, * pwd;

	checkname(pathname);

	inoptr = namei(pathname, &error, 0);
	if(!inoptr)
		return error;

	if(!S_ISDIR(inoptr->i_mode)){
		iput(inoptr);
		return -ENOTDIR;
	}

	/* XXX */
	if(!admit(inoptr, I_RB)){
		iput(inoptr);
		return -EPERM;
	}

	pwd = current->pwd;
	if(pwd == inoptr){
		iput(inoptr);
		return 0;
	}
	
	iput(pwd);

	current->pwd = inoptr;

	inoptr->i_count++;
	iput(inoptr);

	return 0;
}

int do_chroot(const char * pathname)
{
	int error = 0;
	struct inode * inoptr, * root;

	checkname(pathname);

	inoptr = namei(pathname, &error, 0);
	if(!inoptr)
		return error;

	if(!admit(inoptr, I_RB)){
		iput(inoptr);
		return -EPERM;
	}

	root = current->root;
	if(root == inoptr){
		iput(inoptr);
		return 0;
	}
	
	iput(root);

	current->root = inoptr;

	inoptr->i_count++;
	iput(inoptr);

	return 0;
}

int do_getcwd(char * buff, size_t size)
{
	ino_t inum;
	int error = 0, len, idx;
	char name[NAME_MAX + 1];
	struct inode * tmp, * fino;

	if(!buff)
		return -EFAULT;

	tmp = current->pwd;
	tmp->i_count++; // XXX
	ilock(tmp);

	buff[size - 1] = '\0';
	idx = size - 1;

	do{
		inum = tmp->i_num;

		fino = ifind(tmp, "..");
		if(!fino)
			return -EIO;

		if(inum == fino->i_num)
			break;

		error = get_name_of_son(fino, tmp->i_num, name, sizeof(name)); 
		if(error < 0)
			break;

		len = strlen(name);
		if((idx - (len + 1)) < 0){
			error = -ERANGE;
			break;
		}

		idx -= len; 
		memcpy(&buff[idx], name, strlen(name));

		idx--;
		buff[idx] = '/';

		tmp = fino;
	}while(1);

	iput(fino);

	if(!error){
		int i;

		if(idx >= (size - 1)){
			buff[0] = '/';
			buff[1] = '\0';
		}else if(idx > 0){
			for(i = 0; buff[idx]; i++, idx++)
				buff[i] = buff[idx];

			buff[i] = '\0';
		}
	}

	return error;
}
