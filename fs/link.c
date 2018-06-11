#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>

static void freezones(struct super_block * sp, zone_t * zoneptr, int howmuch)
{
	int i;
	zone_t zone;

	for(i = 0; i < howmuch; i++){
		zone = zoneptr[i];
		if(zone != NOZONE){
			inval_block(sp->s_dev, zone << sp->s_lzsize);
			bitfree(sp, (bit_t)zone, ZMAP);
		}
		zoneptr[i] = NOZONE;
	}
}

/*
 * inoptr is locked, and remains in locked state when return
 */
void truncate(struct inode * inoptr)
{
	zone_t zone, zone1, * zoneptr, * zoneptr1;
	int i, scale, ndzones, nindirs;
	struct super_block * sp;
	struct buf_head * bh, * bh1;

	/* char or block device file */
	if(inoptr->i_size <= 0)
		return;

	sp = inoptr->i_sp;
	scale = sp->s_lzsize;
	ndzones = inoptr->i_ndzones;
	nindirs = inoptr->i_nindirs;

	for(i = 0; i < ndzones; i++){
		zone = inoptr->i_zone[i];
		if(zone != NOZONE)
			bitfree(sp, (bit_t)zone, ZMAP);
		inoptr->i_zone[i] = NOZONE;
	}

	if(inoptr->i_size <= (ndzones * (BLOCKSIZ << scale)))
		goto out;

	zone = inoptr->i_zone[ndzones];
	if(zone != NOZONE){
		bh = bread(inoptr->i_dev, (block_t)(zone << scale));
		if(!bh){
			printk("read block error\n");
			goto out;
		}

		zoneptr = (zone_t *)bh->b_data; 

		freezones(sp, zoneptr, nindirs);

#if 0
		bh->b_flags |= B_ASY;
		bwrite(bh);
#else
		bh->b_flags |= B_DIRTY | B_DONE;
		brelse(bh);
#endif

		bitfree(sp, (bit_t)zone, ZMAP);
		inoptr->i_zone[ndzones] = NOZONE;
	}

	if(inoptr->i_size <= ((ndzones + nindirs) * (BLOCKSIZ << scale)))
		goto out;

	zone = inoptr->i_zone[ndzones + 1];
	if(zone != NOZONE){
		bh = bread(inoptr->i_dev, (block_t)(zone << scale));
		if(!bh){
			printk("read block error\n");
			goto out;
		}

		zoneptr = (zone_t *)bh->b_data;
		for(i = 0; i < nindirs; i++){
			zone1 = zoneptr[i];
			if(zone1 != NOZONE){
				bh1 = bread(inoptr->i_dev, (block_t)(zone1 << scale));
				if(!bh1){
					printk("read block error\n");
					goto out;
				}

				zoneptr1 = (zone_t *)bh1->b_data;
				freezones(sp, zoneptr1, nindirs);

				zoneptr[i] = NOZONE;

#if 0
				bh1->b_flags |= B_ASY;
				bwrite(bh1);
#else
				bh1->b_flags |= B_DIRTY | B_DONE;
				brelse(bh1);
#endif

				bitfree(sp, (bit_t)zone1, ZMAP);
			}
		}

#if 0
		bh->b_flags |= B_ASY;
		bwrite(bh);
#else
		bh->b_flags |= B_DIRTY | B_DONE;
		brelse(bh);
#endif

		bitfree(sp, (bit_t)zone, ZMAP);

		inoptr->i_zone[ndzones + 1] = NOZONE;
	}

out:
	inoptr->i_update |= MTIME;
	inoptr->i_size = 0;
	inoptr->i_dirty = 1;
}

int do_link(const char * srcname, const char * destname)
{
	int error = 0;
	char part[NAME_MAX + 1];
	struct inode * inoptr, * inoptr1, * fino;

	checkname(srcname);
	checkname(destname);

	inoptr = namei0(srcname, &error, 0);
	if(!inoptr)
		return error;

	if(S_ISDIR(inoptr->i_mode)){
		iput(inoptr);
		return -EPERM;
	}

	inoptr->i_count++;
	iput(inoptr);

	inoptr1 = namei(destname, &error, 0);
	if(inoptr1){
		iput(inoptr1);
		iput(inoptr);
		return -EEXIST;
	}

	fino = namei(destname, &error, 1);
	if(!fino){
		iput(inoptr);
		return error;
	}

	if(!S_ISDIR(fino->i_mode)){
		iput(fino);
		iput(inoptr);
		return -ENOTDIR;
	}

	if(inoptr->i_dev != fino->i_dev){
		iput(fino);
		iput(inoptr);
		return -EPERM;
	}

	if(!get_tail(destname, part)){
		iput(fino);
		iput(inoptr);
		return -ENAMETOOLONG;
	}

	if(!admit(fino, I_WB)){
		iput(fino);
		iput(inoptr);
		return -EACCES;
	}

	iinsert(fino, part, 0, &error, inoptr->i_num);
	if(error < 0){
		iput(fino);
		iput(inoptr);
		return error;
	}

	inoptr->i_nlinks++;
	inoptr->i_dirty = 1;
	iput(inoptr);

	iput(fino);

	return 0;
}

int do_unlink(const char * pathname)
{
	int error = 0;
	char part[NAME_MAX + 1];
	struct inode * inoptr, * fino;

	checkname(pathname);

	fino = namei(pathname, &error, 1);
	if(!fino)
		return error;

	if(!S_ISDIR(fino->i_mode)){
		iput(fino);
		return -ENOTDIR;
	}

	if(!get_tail(pathname, part)){
		iput(fino);
		return -ENAMETOOLONG;
	}

	fino->i_count++;
	inoptr = ifind(fino, part);
	if(!inoptr){
		iput(fino);
		return -ENOENT;
	}

	if(admit(inoptr, I_WB) == 0){
		iput(inoptr);
		iput(fino);
		return -EPERM;
	}

	if(S_ISDIR(inoptr->i_mode)){
		iput(inoptr);
		iput(fino);
		return -EISDIR;
	}

	inoptr->i_nlinks--;
	if(inoptr->i_nlinks < 0)
		DIE("BUG: links less than zero\n");

	inoptr->i_dirty = 1;
	iput(inoptr);	/* won't block */

	ilock(fino);
	if(!idelete(fino, part, &error)){
		iput(fino);
		return error;
	}

	iput(fino);
	
	return 0;
}

int do_rename(const char * oldpath, const char * newpath)
{
	int error = 0;
	char part[NAME_MAX + 1], part1[NAME_MAX + 1];
	struct inode * inoptr, * fino, * inoptr1, * fino1;

	checkname(oldpath);
	checkname(newpath);

	if(!get_tail(oldpath, part))
		return -ENAMETOOLONG;

	if(!strcmp(part, ".") || !strcmp(part, ".."))
		return -EINVAL;

	fino = namei(oldpath, &error, 1);
	if(!fino)
		return error;

	if(!S_ISDIR(fino->i_mode)){
		iput(fino);
		return -ENOTDIR;
	}

	fino->i_count++;
	inoptr = ifind(fino, part);
	if(!inoptr){
		iput(fino);
		return error;
	}

	// unlock inoptr to avoid dead lock
	inoptr->i_count++;
	iput(inoptr);

	fino1 = namei(newpath, &error, 1);
	if(!fino1){
		iput(fino);
		iput(inoptr);
		return error;
	}
	
	if(!S_ISDIR(fino1->i_mode)){
		iput(fino1);
		iput(fino);
		iput(inoptr);
		return -ENOTDIR;
	}

	if(!get_tail(newpath, part1))
		return -ENAMETOOLONG;

	fino1->i_count++;
	inoptr1 = ifind(fino1, part1);
	if(inoptr1){
		iput(inoptr1);
		iput(fino1);
		iput(fino);
		iput(inoptr);
		return -EEXIST;
	}

	// if newpath is the sub directory of oldpath or not
	if(S_ISDIR(inoptr->i_mode) && bypass(newpath, &error, 1, 1, inoptr)){
		iput(fino1);
		iput(fino);
		iput(inoptr);
		return -EPERM;
	}

	if(inoptr->i_dev != fino1->i_dev){
		iput(fino1);
		iput(fino);
		iput(inoptr);
		return -EPERM;
	}

	if(!admit(fino, I_WB) || !admit(fino1, I_WB)){
		iput(fino1);
		iput(fino);
		iput(inoptr);
		return -EACCES;
	}

	ilock(fino1);

	iinsert(fino1, part1, 0, &error, inoptr->i_num);
	if(error < 0){
		iput(fino1);
		iput(fino);
		iput(inoptr);
		return error;
	}

	if(fino1 != fino){
		fino1->i_count++;
		iput(fino1);
	
		ilock(fino);
	}

	if(!idelete(fino, part, &error)){
		int error1;

		if(fino1 != fino)
			ilock(fino1);

		if(!idelete(fino1, part1, &error1))
			printk("so unlucky!\n");

		iput(fino1);
		iput(fino);
		iput(inoptr);

		return error;
	}

	if(S_ISDIR(inoptr->i_mode)){
		fino1->i_nlinks++;
		fino1->i_update |= MTIME;
		fino1->i_dirty = 1;

		fino->i_nlinks--;
		fino->i_update |= MTIME;
		fino->i_dirty = 1;
	}

	iput(fino1);
	iput(fino);

	iput(inoptr);

	return 0;
}

int do_symlink(const char * oldpath, const char * newpath)
{
	int error = 0, len, ret;
	char part[NAME_MAX + 1];
	struct inode * inoptr, * fino;

	checkname(oldpath);
	checkname(newpath);

	len = strlen(oldpath);
	if(len >= 4096)
		return -ENAMETOOLONG;

	inoptr = namei(newpath, &error, 0);
	if(inoptr){
		iput(inoptr);
		return -EEXIST;
	}

	fino = namei(newpath, &error, 1);
	if(!fino)
		return error;

	if(!S_ISDIR(fino->i_mode)){
		iput(fino);
		return -ENOTDIR;
	}


	if(!get_tail(newpath, part)){
		iput(fino);
		return -ENAMETOOLONG;
	}

	if(!admit(fino, I_WB)){
		iput(fino);
		return -EACCES;
	}

	inoptr = iinsert(
		fino, part, (0755 & (~current->umask)) | I_LNK, &error, 0
		);
	if(!inoptr){
		iput(fino);
		return error;
	}

	ret = 0;

	// just for simple, inoptr1 is locked and will be unlocked in do_close
	{
		int fd;

		ret = do_inode_open(inoptr, O_WRONLY);
		if(ret < 0)
			goto errout;

		fd = ret;

		ret = do_write(fd, oldpath, len);
		if(ret != len){
			do_close(fd);
			goto errout;
		}

		do_close(fd);

		ret = 0;
	}

errout:
	iput(inoptr);
	iput(fino);

	return ret;
}

int do_readlink(const char * path, char * buff, size_t buf_size)
{
	int error = 0;
	off_t off;
	block_t block;
	struct inode * inoptr;
	struct buf_head * bh;

	checkname(path);

	inoptr = namei0(path, &error, 0);
	if(!inoptr)
		return error;

	if(!S_ISLNK(inoptr->i_mode)){
		iput(inoptr);
		return -EPERM;
	}

	if(!admit(inoptr, I_RB)){
		iput(inoptr);
		return -EACCES;
	}

	inoptr->i_count++;
	iput(inoptr); // XXX: unlock first

	off = 0;
	while((off < buf_size) && (off < inoptr->i_size)){
		int len = BLOCKSIZ;

		if(len > inoptr->i_size - off)
			len = inoptr->i_size - off;

		if(len > buf_size - off)
			len = buf_size - off;

		block = bmap(inoptr, off);

		if(block == NOBLOCK)
			memset(&buff[off], 0, BLOCKSIZ);
		else{
			bh = bread(inoptr->i_dev, block);
			if(!bh){
				iput(inoptr);
				return -EIO;
			}

			memcpy(&buff[off], bh->b_data, len);
			brelse(bh);
		}

		off += len;
	}

	iput(inoptr);

	return off;
}
