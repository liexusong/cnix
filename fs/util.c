#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>
#include <cnix/mm.h>

/* Notice: zone_t will only support V2 */
#define HOWMUCH	(BLOCKSIZ / (zone_t))

#define get_zone(data, index) (((zone_t *)data)[index])

/* 
 * return value:
 *   1 ok
 *   0 fail
 */
int admit(struct inode * inoptr, mode_t expect)
{
	mode_t mode;
	uid_t euid;
	gid_t egid;

	mode = inoptr->i_mode;

	euid = current->euid;
	egid = current->egid;

	/* super user */
	if(euid == SU_UID){
		/* expect executable, but not a executable file */
		if((expect & 1) && !(mode & (1 + 8 + 64)))
			return 0;

		return 1;
	}

	/* owner */
	if((euid == inoptr->i_uid) && ((expect << 6) & mode))
		return 1;

	/* same group */
	if((egid == inoptr->i_gid) && ((expect << 3) & mode))
		return 1;

	/* other user */
	if(expect & mode)
		return 1;

	return 0;
}

/* 
 * return value:
 *   1 ok
 *   0 fail
 */
int minode(struct inode * inoptr, off_t pos, zone_t zone)
{
	block_t block;
	zone_t zone_pos, tmpzone, excess;
	int scale, dzones, nindirs, index;
	struct buf_head * bh;

	scale = inoptr->i_sp->s_lzsize;
	zone_pos = (pos / BLOCKSIZ) >> scale;

	/* direct zones */
	dzones = inoptr->i_ndzones;

	if(zone_pos < dzones){
		inoptr->i_zone[zone_pos] = zone;
		inoptr->i_update |= MTIME;
		inoptr->i_dirty = 1;

		return 1;
	}

	/* indirect zones */
	nindirs = inoptr->i_nindirs;

	excess = zone_pos - dzones;
	if(excess < nindirs){
		/* single indirect */
		tmpzone = inoptr->i_zone[dzones];
		if(tmpzone == NOZONE){
			/*
			 * file size is just lager than single indirect
			 * excess may be not zero if lseek to after file size
			 */
			//if(excess != 0)
			//	DIE("BUG: excess must be zero\n");

			tmpzone = (zone_t)bitalloc(inoptr->i_sp, ZMAP);
			if(tmpzone == NOZONE){
				printk("out of disk space\n");
				return 0;
			}

			inoptr->i_zone[dzones] = tmpzone;

			block = tmpzone << scale;

			/* allocate a block and clear data */	
			bh = getblk(inoptr->i_dev, block);

			/* getblk won't fail */
			memset(bh->b_data, 0, BLOCKSIZ);
		}else{
			block = tmpzone << scale;

			/* XXX: is zone size the same with block size? */
			bh = bread(inoptr->i_dev, block);
			if(!bh){
				printk("error when reading block\n");
				return 0;
			}
		}
	
		get_zone(bh->b_data, excess) = zone;
		bh->b_flags |= B_DIRTY | B_DONE;
		brelse(bh);

		inoptr->i_update |= MTIME;
		inoptr->i_dirty = 1;

		return 1;
	}

	/* double indirect offset */
	excess -= nindirs;
	index = excess / nindirs;

	/* double indirect */
	tmpzone = inoptr->i_zone[dzones + 1];
	if(tmpzone == NOZONE){
		tmpzone = (zone_t)bitalloc(inoptr->i_sp, ZMAP);
		if(tmpzone == NOZONE){
			printk("out of disk space\n");
			return 0;
		}

		inoptr->i_zone[dzones + 1] = tmpzone;
		inoptr->i_update |= MTIME;
		inoptr->i_dirty = 1;

		/*
		 * file size is just lager than signle indirect
		 * excess may be not zero if lseek to after file size
		 */
		//if(index != 0)
		//	DIE("BUG: index must be zero\n");

		block = tmpzone << scale;

		/* allocate a block and clear data */	
		bh = getblk(inoptr->i_dev, block);

		/* getblk won't fail */
		memset(bh->b_data, 0, BLOCKSIZ);

		/* because won't be freed, diffirent from write */
		bh->b_flags |= B_DIRTY | B_DONE;
	}else{
		block = tmpzone << scale;

		/* XXX: is zone size the same with block size? */
		bh = bread(inoptr->i_dev, block);
		if(!bh){
			printk("error when reading block\n");
			return 0;
		}
	}

	tmpzone = get_zone(bh->b_data, index);
	if(tmpzone == NOZONE){
		tmpzone = (zone_t)bitalloc(inoptr->i_sp, ZMAP);
		if(tmpzone == NOZONE){
			printk("out of disk space\n");

			inoptr->i_update |= MTIME;
			inoptr->i_dirty = 1;
			brelse(bh);

			return 0;
		}

		get_zone(bh->b_data, index) = tmpzone;
		bh->b_flags |= B_DIRTY | B_DONE;
		brelse(bh);

		block = tmpzone << scale;

		/* allocate a block and clear data */	
		bh = getblk(inoptr->i_dev, block);

		/* getblk won't fail */
		memset(bh->b_data, 0, BLOCKSIZ);
	}else{
		brelse(bh);

		block = tmpzone << scale;

		/* XXX: is zone size the same with block size? */
		bh = bread(inoptr->i_dev, block);
		if(!bh){
			printk("error when reading block\n");
			return 0;
		}
	}

	excess %= nindirs;
	if(get_zone(bh->b_data, excess) != NOZONE)
		DIE("cannot happen\n");

	get_zone(bh->b_data, excess) = zone;

	bh->b_flags |= B_DIRTY | B_DONE;
	brelse(bh);

	inoptr->i_update |= MTIME;
	inoptr->i_dirty = 1;

	return 1;
}

block_t bmap(struct inode * inoptr, off_t pos)
{
	block_t block_pos, block;
	zone_t zone_pos, zone, excess;
	int scale, block_off, dzones, nindirs, index;
	struct buf_head * bh;

	scale = inoptr->i_sp->s_lzsize;
	block_pos = pos / BLOCKSIZ;
	zone_pos = block_pos >> scale;
	block_off = block_pos - (zone_pos << scale);

	dzones = inoptr->i_ndzones;
	nindirs = inoptr->i_nindirs;

	if(zone_pos < dzones){
		 zone = inoptr->i_zone[zone_pos];
		 if(zone == NOZONE)
			 return NOBLOCK;

		 block = (zone << scale) + block_off;

		 return block;
	}

	excess = zone_pos - dzones;

	/* single indirect */
	if(excess < nindirs)
		zone = inoptr->i_zone[dzones];
	else{
		/* double indirect */
		zone = inoptr->i_zone[dzones + 1];
		if(zone == NOZONE)
			return NOBLOCK;

		/* double indirect offset */
		excess -= nindirs;

		block = zone << scale;
		bh = bread(inoptr->i_dev, block);
		if(!bh){
			printk("error when reading block\n");
			return NOBLOCK;
		}

		index = (excess / nindirs);
		zone = get_zone(bh->b_data, index);

		brelse(bh);

		excess %= nindirs;
	}

	if(zone == NOZONE)
		return NOBLOCK;

	block = zone << scale;

	bh = bread(inoptr->i_dev, block);
	if(!bh){
		printk("error when reading block\n");
		return NOBLOCK;
	}

	zone = get_zone(bh->b_data, excess);

	brelse(bh);

	if(zone == NOZONE)
		return NOBLOCK;

	block = (zone << scale) + block_off;

	return block;
}

int iempty(struct inode * inoptr)
{
	int i;
	off_t pos;
	block_t block;
	struct buf_head * bh;
	struct direct * dirptr;

	if(!S_ISDIR(inoptr->i_mode))
		DIE("BUG: inoptr must be a directory\n");

	for(pos = 0; pos < inoptr->i_size; pos += BLOCKSIZ){
		block = bmap(inoptr, pos);
		if(block == NOBLOCK){
			printk("can't get NOBLOCK\n");
			return 0; /* XXX: or 1 */
		}

		bh = bread(inoptr->i_dev, block);
		if(!bh){
			printk("error when reading block\n");
			return 0; /* XXX: or 1 */
		}
	
		dirptr = (struct direct *)bh->b_data;
		for(i = 0; i < (BLOCKSIZ / DIRECT_SIZE); i++, dirptr++)
			if(dirptr->d_ino 
				&& strncmp(".", dirptr->d_name, NAME_MAX)
				&& strncmp("..", dirptr->d_name, NAME_MAX)){
				brelse(bh);
				return 0;
		}

		brelse(bh);
	}

	return 1;
}

/*
 * return a locked inode, ialloc only if ino is zero,
 * inoptr is a locked inode, and remains in locked state when return,
 */
struct inode * iinsert(
	struct inode * inoptr,
	const char * name,
	mode_t mode,
	int * error,
	ino_t ino)
{
	int i;
	off_t pos;
	block_t block;
	struct direct * dirptr;
	struct buf_head * bh;
	struct inode * tmp;

	if(!S_ISDIR(inoptr->i_mode))
		DIE("BUG: inoptr must be a directory\n");

	/* this flag must be checked outside */
	if(inoptr->i_sp->s_roflag){
		*error = -EROFS;
		return NULL;
	}

	if(strlen(name) > NAME_MAX){
		*error = -ENAMETOOLONG;
		return NULL;
	}

	/* avoid gcc warnning */
	tmp = NULL; 
	i = 0;
	bh = NULL;
	dirptr = NULL;

	for(pos = 0; pos < inoptr->i_size; pos += BLOCKSIZ){
		block = bmap(inoptr, pos);
		if(block == NOBLOCK){
			printk("can't get NOBLOCK\n");
			*error = -EIO;
			return NULL;
		}

		bh = bread(inoptr->i_dev, block);
		if(!bh){
			printk("error when reading block\n");
			*error = -EIO;
			return NULL;
		}
	
		dirptr = (struct direct *)bh->b_data;
		for(i = 0; i < (BLOCKSIZ / DIRECT_SIZE); i++, dirptr++){
			if(dirptr->d_ino != 0)
				continue;

			if(pos + (i + 1) * DIRECT_SIZE > inoptr->i_size){
				if(pos + i * DIRECT_SIZE != inoptr->i_size){
					brelse(bh);
					printk("bug when add a dir item\n");
					*error = -EIO;
					return NULL;
				}
			}

			if(ino == 0){
				tmp = ialloc(inoptr->i_dev, mode, error);
				if(!tmp){
					brelse(bh);

					printk("out of disk space\n");
					*error = -ENOSPC;
					return NULL;
				}
			}

		nowdoit:
			strncpy(dirptr->d_name, name, NAME_MAX);
			dirptr->d_ino = (ino == 0) ? tmp->i_num : ino;

#if 0
			bh->b_flags |= B_ASY;
			bwrite(bh);
#else
			bh->b_flags |= B_DIRTY | B_DONE;
			brelse(bh);
#endif

			if(pos + (i + 1) * DIRECT_SIZE > inoptr->i_size)
				inoptr->i_size += DIRECT_SIZE;

			inoptr->i_update |= MTIME;
			inoptr->i_dirty = 1;

			return (ino == 0) ? tmp : NULL;	
		}

		brelse(bh);
	}

	if(inoptr->i_size >= FILESIZE_MAX){
		*error = -EFBIG;
		return NULL;
	}

	block = (block_t)bitalloc(inoptr->i_sp, ZMAP);
	if(block == NOZONE){
		printk("out of disk space\n");
		*error = -ENOSPC;
		return NULL;
	}

	bh = getblk(inoptr->i_dev, block);

	memset(bh->b_data, 0, BLOCKSIZ);
	dirptr = (struct direct *)bh->b_data;

	if(ino == 0){
		tmp = ialloc(inoptr->i_dev, mode, error);
		if(!tmp){
			brelse(bh);
			bitfree(inoptr->i_sp, block, ZMAP);

			printk("out of disk space\n");
			*error = -ENOSPC;
			return NULL;
		}
	}

	if(minode(inoptr,
		pos, (zone_t)(block >> inoptr->i_sp->s_lzsize)) == 0){
		brelse(bh);
		bitfree(inoptr->i_sp, block, ZMAP);
		if(ino == 0)
			iput(tmp);
		*error = -ENOSPC;
		return NULL;
	}

	i = 0;

	goto nowdoit;
}

/*
 * inoptr is a locked inode
 */
int idelete(struct inode * inoptr, const char * name, int * error)
{
	int i;
	off_t pos;
	block_t block;
	struct buf_head * bh;
	struct direct * dirptr;

	if(!S_ISDIR(inoptr->i_mode))
		DIE("BUG: inoptr must be a directory\n");

	if(inoptr->i_sp->s_roflag){
		*error = -EROFS;
		return 0;
	}

	if(strlen(name) > NAME_MAX){
		*error = -ENAMETOOLONG;
		return 0;
	}

	if((strncmp(name, ".", NAME_MAX) == 0)
		|| (strncmp(name, "..", NAME_MAX) == 0)){
		*error = -EPERM;
		return 0;
	}

	for(pos = 0; pos < inoptr->i_size; pos += BLOCKSIZ){
		block = bmap(inoptr, pos);
		if(block == NOBLOCK){
			printk("can't get NOBLOCK\n");
			*error = -EIO;
			return 0;
		}

		bh = bread(inoptr->i_dev, block);
		if(!bh){
			printk("error when reading block\n");
			*error = -EIO;
			return 0;
		}
	
		dirptr = (struct direct *)bh->b_data;
		for(i = 0; i < (BLOCKSIZ / DIRECT_SIZE); i++, dirptr++){
			if(dirptr->d_ino == 0)
				continue;

			if(strncmp(name, dirptr->d_name, NAME_MAX) == 0){
				dirptr->d_ino = 0;
#if 0
				bh->b_flags |= B_ASY;
				bwrite(bh);
#else
				bh->b_flags |= B_DIRTY | B_DONE;
				brelse(bh);
#endif
				return 1;
			}
		}

		brelse(bh);
	}

	*error = -ENOENT;

	return 0;
}

/*
 * return a locked inode,
 * inoptr is a locked inode, and will be iput here always
 */
struct inode * ifind(struct inode * inoptr, const char * name)
{
	int i, count;
	ino_t ino;
	off_t pos;
	block_t block;
	struct direct * dirptr;
	struct buf_head * bh;
	struct super_block * sp;
	struct inode * tmp, * workino;

	if(!S_ISDIR(inoptr->i_mode)){
		iput(inoptr); /* XXX */
		return NULL;
	}

	if((inoptr->i_num == ROOT_INODE) && (strcmp("..", name) == 0)){
		if(inoptr == task[INITPID]->root) /* it's '/' inode */
			return inoptr;

		sp = inoptr->i_sp;
		if(!sp)
			DIE("cannot happen\n");

		workino = sp->s_mounti;

		workino->i_count++;
		ilock(workino);

		iput(inoptr);
	}else
		workino = inoptr;

	ino = 0;

	for(pos = 0; pos < workino->i_size; pos += BLOCKSIZ){
		block = bmap(workino, pos);
		if(block == NOBLOCK){
			printk("can't get NOBLOCK\n");
			iput(workino); /* XXX */
			return NULL;
		}

		bh = bread(workino->i_dev, block);
		if(!bh){
			printk("error when reading block\n");
			iput(workino); /* XXX */
			return NULL;
		}
	
		count = BLOCKSIZ / DIRECT_SIZE;
		if(workino->i_size - pos < BLOCKSIZ)
			count = (workino->i_size - pos) / DIRECT_SIZE;

		dirptr = (struct direct *)bh->b_data;
		for(i = 0; i < count; i++, dirptr++){
			if(dirptr->d_ino == 0)
				continue;

			if(strncmp(name, dirptr->d_name, NAME_MAX) == 0){
				ino = dirptr->d_ino;
				break;
			}
		}

		if(ino != 0){
			brelse(bh);
			break;
		}

		brelse(bh);
	}

	if(ino != 0){
		/* won't be blocked, so can use it below immediately */
		iput(workino); 

		tmp = iget(workino->i_dev, ino);
		if(tmp->i_flags & MOUNT_I){ /* dir is mounted by device */
			sp = tmp->i_mounts;
			iput(tmp);

			tmp = sp->s_rooti;
			tmp->i_count++;
			ilock(tmp);
		}

		return tmp;
	}

	iput(workino);

	return NULL;
}

int real_filename(char * page, struct inode * inoptr, int * error)
{
	off_t off;
	struct buf_head * bh;

	if(inoptr->i_size >= PAGE_SIZE){
		*error = -ENAMETOOLONG;
		return 0;
	}

	off = 0;
	while((off < PAGE_SIZE) && (off < inoptr->i_size)){
		block_t block = bmap(inoptr, off);

		if(block == NOBLOCK)
			memset(&page[off], 0, BLOCKSIZ);
		else{
			bh = bread(inoptr->i_dev, block);
			if(!bh){
				*error = -EIO;
				return 0;
			}

			memcpy(&page[off], bh->b_data, BLOCKSIZ);
			brelse(bh);
		}

		off += BLOCKSIZ;
	}

	page[inoptr->i_size] = '\0';

	return 1;
}

/*
 * return value:
 *   NULL: end of name
 *     -1: name is too long
 *    oth: next char
 */
static char * get_next_name(char * nextch, char * part, int flag)
{
	int length;
	char * prevch;

	if(!nextch[0])
		return NULL;

	prevch = nextch;
	nextch = strchr(nextch, '/');

	if(nextch){
		length = nextch - prevch;

		while(nextch[0] == '/')
			nextch++;
	}else{
		length = strlen(prevch);
		nextch = &prevch[length];
	}

	if(!length)
		return NULL;

	if(length > NAME_MAX)
		return (char *)-1;

	if(flag && !nextch[0])
		return NULL;

	memcpy(part, prevch, length);
	part[length] = '\0';

	return nextch;
}

#define MAX_SYMLINK_DEPTH 32

/* 
 * flag:
 *   0 find filename's inode
 *   1 find filename's father's inode
 * return a locked inode
 */
static struct inode * __namei(
	const char * filename, int * error, int flag, int into_link,
	struct inode * inoptr
	)
{
	dev_t prevdev;
	ino_t previno;
	int level, end_with_slash;
	struct inode * workino, * foundino;
	char * nextch, * prevch, part[NAME_MAX + 1], * page, * page1;

	level = 0;
	end_with_slash = 0;
	prevdev = current->pwd->i_dev;
	previno = current->pwd->i_num;

	page = (char *)get_one_page();
	if(!page){
		*error = -ENOMEM;
		return NULL;
	}

	page1 = (char *)get_one_page();
	if(!page1){
		free_one_page((unsigned long)page);
		*error = -ENOMEM;
		return NULL;
	}

	nextch = (char *)filename;

step_into_link:

	if(!nextch[0])
		goto errout1;	

	if(nextch[strlen(nextch) - 1] == '/')
		end_with_slash = 1;
	
	if(nextch[0] == '/'){
		// skip '/' before calling get_next_name
		while(nextch[0] == '/')
			nextch++;

		prevdev = current->root->i_dev;
		previno = current->root->i_num;
	}

	workino = iget(prevdev, previno);

	*error = 0;

	do{
		// bypass
		if(inoptr && (workino == inoptr))
			break;

		prevch = nextch;
		nextch = get_next_name(nextch, part, flag);

		if(S_ISLNK(workino->i_mode)){
			if(!nextch && !into_link)
				break;

			if(level > MAX_SYMLINK_DEPTH){
				*error = -EMLINK;
				goto errout;
			}

			level++;

			if(!real_filename(page1, workino, error))
				goto errout;

			if(nextch){
				// just for simple
				if(PAGE_SIZE - strlen(page1)
					< strlen(prevch) + 1 + 1){
					*error = -ENAMETOOLONG;
					goto errout;
				}

				strcat(page1, "/");
				strcat(page1, prevch);
			}

			// switch page and page1
			{
				char * tmp;

				tmp = page;
				page = page1;
				page1 = tmp;
			}

			iput(workino);

			nextch = page;
			goto step_into_link;
		}

		/*
                 * "XXX/", "XXXXXXX/////"
		 *     ^           ^^^^^ 
	 	 */
		if(!nextch){
			// must be a directory
			if(end_with_slash && !S_ISDIR(workino->i_mode)){
				*error = -ENOTDIR;
				goto errout;
			}

			break;
		}

		if(nextch == (char *)-1){
			*error = -ENAMETOOLONG;
			goto errout;
		}

		/* XXX, readble or excutable? */
		if(!admit(workino, I_XB)){
			*error = -EACCES;
			goto errout;
		}

		prevdev = workino->i_dev;
		previno = workino->i_num;

		/* ifind will iput workino */
		foundino = ifind(workino, part);
		if(!foundino){
			*error = -ENOENT;
			goto errout1;
		}

		workino = foundino;
	}while(1);

	free_one_page((unsigned long)page);
	free_one_page((unsigned long)page1);

	return workino;

errout:
	iput(workino);
errout1:
	free_one_page((unsigned long)page);
	free_one_page((unsigned long)page1);

	return NULL;
}

/* 
 * flag:
 *   0 find filename's inode
 *   1 find filename's father's inode
 * return a locked inode
 */
struct inode * namei(const char * filename, int * error, int flag)
{
	return __namei(filename, error, flag, 1, NULL);
}

/* 
 * flag:
 *   0 find filename's inode
 *   1 find filename's father's inode
 * return a locked inode
 */
struct inode * namei0(const char * filename, int * error, int flag)
{
	return __namei(filename, error, flag, 0, NULL);
}

/* 
 * flag:
 *   0 check filename
 *   1 check filename's father
 * inoptr:
 *   a unlocked inode
 * return:
 *   0: not bypass
 *   1: bypass
 */
int bypass(
	const char * filename, int * error, int flag, int into_link,
	struct inode * inoptr
	)
{
	int ret = 0;
	struct inode * inoptr1;

	inoptr1 = __namei(filename, error, flag, 1, inoptr);
	if(inoptr1){
		if(inoptr == inoptr1)
			ret = 1;

		iput(inoptr1);
	}

	return ret;
}

int get_tail(const char *pathname, char *part)
{
	const char *ptr;
	const char *p;
	int len;

	if (!(len = strlen(pathname)))
	{
		return 0;
	}

	// get the first char which is not '/' from the end
	p = &pathname[len - 1];
	while (*p == '/' && p > pathname)
	{
		p--;
	}

	if (*p == '/')	// all are '/'
	{
		return 0;
	}

	// get the second '/' from the end
	ptr = p;
	while (*ptr != '/' && ptr > pathname)
	{
		ptr--;
	}

	if (*ptr == '/')
	{
		ptr++;
	}

	if ((len = p - ptr + 1) > NAME_MAX)
	{
		return 0;
	}

	memcpy(part, ptr, len);
	part[len] = '\0';

	return 1;
}

int get_name_of_son(struct inode * fino, ino_t inum, char * name, int size)
{
	int i;
	off_t pos;
	block_t block;
	struct buf_head * bh;
	struct direct * dirptr;
	struct inode * tmp;

	tmp = fino;

	for(pos = 0; pos < tmp->i_size; pos += BLOCKSIZ){
		block = bmap(tmp, pos);
		if(block == NOBLOCK){
			printk("can't get NOBLOCK\n");
			return -EIO;
		}

		bh = bread(tmp->i_dev, block);
		if(!bh){
			printk("error when reading block\n");
			return -EIO;
		}
	
		dirptr = (struct direct *)bh->b_data;
		for(i = 0; i < (BLOCKSIZ / DIRECT_SIZE); i++, dirptr++){
			if(dirptr->d_ino != inum)
				continue;

			strncpy(name, dirptr->d_name, size);

			brelse(bh);
			return 0;
		}

		brelse(bh);
	}

	return -ENOENT; /* wierd, not possible */
} 


#define DIRNSIZE	(NR_DZONES * ZONE_SIZE)
#define INDIRNSIZE	(NR_DZONES * ZONE_SIZE + NR_INDIRECTS * ZONE_SIZE)
#define DINDIRNSIZE	(FILESIZE_MAX)
#define BLOCKS		(ZONE_SIZE / 512)
size_t calc_block_nr(size_t length)
{
	size_t ret;
	size_t tmp = length;
	
	if (length == 0)
	{
		return 0;
	}

	if (length <= DIRNSIZE)
	{
		return ((length + ZONE_SIZE - 1) / ZONE_SIZE) * BLOCKS;
	}

	ret = NR_DZONES;
	if (length <= INDIRNSIZE)
	{
		tmp -= DIRNSIZE;
		// the number of single indrect block.
		ret++;
		
		ret += (tmp + ZONE_SIZE - 1) / ZONE_SIZE;

		return ret * BLOCKS;		
	}

	if (length > FILESIZE_MAX)
	{
		return (FILESIZE_MAX / ZONE_SIZE) * BLOCKS;
	}

	// the number of single indirect block is ONE.
	ret++;		
	tmp -= INDIRNSIZE;

	// the number of double indirect block.
	ret += (tmp +  NR_INDIRECTS * ZONE_SIZE - 1) / (NR_INDIRECTS * ZONE_SIZE);

	// the acutual block of data.
	ret += (tmp + ZONE_SIZE - 1) / ZONE_SIZE;

	return ret * BLOCKS;
}
