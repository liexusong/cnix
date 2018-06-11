#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>
#include <cnix/mm.h>

static struct inode inodes[NR_INODE];
static list_t inode_hash[NR_INODE_HASH];

static list_t ifreelist;
int inode_free_num;
static struct wait_queue * iflwaitq = NULL; /* ifreelist wait queue */

#define ihashfn(dev, ino) (&inode_hash[((dev) ^ (ino)) % NR_INODE_HASH])

static struct inode * get_ihash(dev_t, ino_t);
static void put_ihash(struct inode *);
static void iread(struct inode * );
static void iwrite(struct inode *);

#define ifree(sp, ino) bitfree(sp, ino, IMAP)

/*
 * TODO:
 *   setup a timer to flush inodes in some interval, flash all when shutdown
 */
void init_inode(void)
{
	int i;
	struct inode * inoptr;

	list_head_init(&ifreelist);
	inode_free_num = NR_INODE;

	for(i = 0; i < NR_INODE; i++){
		inoptr = &inodes[i];

		/* i_wait will be set to NULL here */
		memset(inoptr, 0, sizeof(struct inode));

		inoptr->i_dev = NODEV;	/* invalid */
		inoptr->i_count = 0;	/* free */

		list_add_head(&ifreelist, &inoptr->i_freelist);
		list_head_init(&inoptr->i_hashlist);
	}

	for(i = 0; i < NR_INODE_HASH; i++)
		list_head_init(&inode_hash[i]);
}

static struct inode * get_ihash(dev_t dev, ino_t ino)
{
	list_t * hash, * tmp, * pos;
	struct inode * inoptr;

	hash = ihashfn(dev, ino);
	list_foreach(tmp, pos, hash){
#if 1
		if(tmp->next == tmp)
			DIE("BUG: dev %d ino %d, cannot happen\n", dev, ino);
#endif
		inoptr = list_entry(tmp, i_hashlist, struct inode);
		if((inoptr->i_dev == dev) && (inoptr->i_num == ino))
			return inoptr;
	}

	return NULL;
}

static void put_ihash(struct inode * inoptr)
{
	list_t * hash;

	hash = ihashfn(inoptr->i_dev, inoptr->i_num);
	list_add_head(hash, &inoptr->i_hashlist);
}

void ilock(struct inode * inoptr)
{
loop:
	if(inoptr->i_flags & LOCK_I){
		if(inoptr->i_lockowner == current->pid)
			DIE("BUG: cannot happen\n");

		inoptr->i_flags |= WANT_I;
		sleepon(&inoptr->i_wait);
		goto loop;
	}

	inoptr->i_flags |= LOCK_I;
	inoptr->i_lockowner = current->pid;
}

/*
 * return a LOCKED inode
 */
struct inode * iget(dev_t dev, ino_t ino)
{
	struct inode * tmp;
	struct super_block * sp;

	tmp = NULL;

loop:
	if(dev != NODEV)
		tmp = get_ihash(dev, ino);

	if(tmp){
		if(tmp->i_flags & LOCK_I){
			tmp->i_flags |= WANT_I;
			sleepon(&tmp->i_wait);
			goto loop;
		}

		/* XXX */
		if(tmp->i_flags & MOUNT_I){
			sp = tmp->i_mounts;
			/* avoid looping when iget root inode*/
			if(sp->s_rooti->i_num != ino){
				ino = sp->s_rooti->i_num;
				goto loop;
			}
		}

		/* if it's in freelist, dequeue it */
		if(tmp->i_count == 0){
			list_del1(&tmp->i_freelist);
			inode_free_num--;
		}

		tmp->i_flags |= LOCK_I;
		tmp->i_lockowner = current->pid;
		tmp->i_count++;

		return tmp;
	}

loop1:
	if(list_empty(&ifreelist)){
		sleepon(&iflwaitq);
		goto loop1;
	}

	tmp = list_entry(ifreelist.next, i_freelist, struct inode);
	if(tmp->i_dev != NODEV)
		list_del1(&tmp->i_hashlist);

	list_del1(&tmp->i_freelist);

	/* XXX */
	if(tmp->i_dirty){
		if(tmp->i_dev != NODEV)
			iwrite(tmp);

		goto loop;
	}

	tmp->i_dev = dev;	

	/* NODEV for pipe and new inode */
	if(dev != NODEV){
		tmp->i_num = ino;

		iread(tmp);

		tmp->i_ndzones = NR_DZONES;
		tmp->i_nindirs = NR_INDIRECTS;
	}

	tmp->i_count = 1;

	/* tmp->i_sp has been filled in iread, or pipe needn't to initialize */
	tmp->i_dirty = 0;
	tmp->i_flags = LOCK_I;
	tmp->i_lockowner = current->pid;

	/* the same reason as b_wait in function of buffer.c */
	/*tmp->i_wait = NULL;*/

	tmp->i_mounts = NULL;
	tmp->i_update = 0;

	/* after inode's data is all filled */
	if(dev != NODEV)
		put_ihash(tmp);

	inode_free_num--;

	return tmp;
}

extern void socket_close(struct inode * inoptr);

/*
 * if inode is locked, must be locked by who called iput
 */
void iput(struct inode * inoptr)
{
	struct super_block * sp;

	if(!inoptr)
		DIE("BUG: put a NULL inode\n");

	if(!(inoptr->i_flags & LOCK_I)
		|| (inoptr->i_lockowner != current->pid))
		ilock(inoptr);

	--inoptr->i_count;
	if(inoptr->i_count == 1){
		if(inoptr->i_flags & PIPE_I){
			inoptr->i_buddy = 0;
			wakeup(&inoptr->i_wait);
			select_wakeup(
				&inoptr->i_select, OPTYPE_READ | OPTYPE_READ
				);
		}
	}else if(inoptr->i_count == 0){
		if(inoptr->i_dev != NODEV){
			/* this inoptr is invalid, must be dequeued from hash */
			if(list_empty(&inoptr->i_hashlist))
				DIE("BUG: inode must be in hash list\n");

			if(inoptr->i_nlinks == 0){
				truncate(inoptr);

				sp = sread(inoptr->i_dev);
				ifree(sp, inoptr->i_num);

				inoptr->i_mode = 0;
				list_del1(&inoptr->i_hashlist);
			}
		}else if(inoptr->i_flags & PIPE_I){
			if(inoptr->i_buffer){
				free_one_page((unsigned long)inoptr->i_buffer);
				inoptr->i_buffer = NULL;
			}
		}else if(S_ISSOCK(inoptr->i_mode))
			socket_close(inoptr);

		/* add inoptr into free list tail */
		list_add_tail(&ifreelist, &inoptr->i_freelist);
		inode_free_num++;

		wakeup(&iflwaitq);
	}else if(inoptr->i_count < 0)
		DIE("BUG: i_count is less than 0, dev %d ino %d\n",
			inoptr->i_dev, inoptr->i_num);

	if(inoptr->i_dirty && (inoptr->i_dev != NODEV))
		iwrite(inoptr);

	inoptr->i_flags &= ~LOCK_I;
	inoptr->i_lockowner = 0;

	if(inoptr->i_flags & WANT_I){
		inoptr->i_flags &= ~WANT_I;
		wakeup(&inoptr->i_wait);
	}
}

#define icopy(dest, src) \
do{ \
	dest->i_mode = src->i_mode; \
	dest->i_nlinks = src->i_nlinks; \
	dest->i_uid = src->i_uid; \
	dest->i_gid = src->i_gid; \
	dest->i_size = src->i_size; \
	dest->i_atime = src->i_atime; \
	dest->i_mtime = src->i_mtime; \
	dest->i_ctime = src->i_ctime; \
	for(i = 0; i < NR_ZONES; i++) \
		dest->i_zone[i] = src->i_zone[i]; \
}while(0)

/*
 * inoptr is not locked
 */
static void iread(struct inode * inoptr)
{
	int i;
	ino_t ino;
	block_t block;
	struct buf_head * bh;
	struct super_block * sp;
	struct inode * tmp;

	ino = inoptr->i_num;

	sp = sread(inoptr->i_dev);

	/* 2 for boot and super block */
	block = 2 + sp->s_imblocks + sp->s_zmblocks;	

	/* 0 inode number is saved, but inode, use it */
	block += (ino - 1) / sp->s_ipblock;

	bh = bread(inoptr->i_dev, block);
	if(!bh){
		DIE("error happen when read inode!\n");
		return;
	}

	/* "- 1" for number-0 which is not-used */
	tmp = (struct inode *)
		(&bh->b_data[((ino - 1) % sp->s_ipblock) * INODE_SIZE]);

	icopy(inoptr, tmp);

	inoptr->i_sp = sp;

	brelse(bh);
}

/*
 * inoptr is not locked
 */
static void iwrite(struct inode * inoptr)
{
	int i;
	ino_t ino;
	block_t block;
	struct buf_head * bh;
	struct super_block * sp;
	struct inode * tmp;

	ino = inoptr->i_num;

	sp = sread(inoptr->i_dev);

	block = 2 + sp->s_imblocks + sp->s_zmblocks;

	/* number-0 is reserved */
	block += (ino - 1) / sp->s_ipblock;
	bh = bread(inoptr->i_dev, block);
	if(!bh){
		DIE("error happen when write inode!\n");
		return;
	}

	tmp = (struct inode *)
		(&bh->b_data[((ino - 1) % sp->s_ipblock) * INODE_SIZE]);

	if(inoptr->i_update){
		time_t now;

		now = curclock();

		if(inoptr->i_update & ATIME)
			inoptr->i_atime = now;

		if(inoptr->i_update & MTIME)
			inoptr->i_mtime = now;

		if(inoptr->i_update & CTIME)
			inoptr->i_ctime = now;
	}

	icopy(tmp, inoptr);

#if 0
	bh->b_flags |= B_ASY;
	bwrite(bh);
#else
	bh->b_flags |= B_DIRTY | B_DONE;
	brelse(bh);
#endif

	inoptr->i_dirty = 0;
	inoptr->i_update = 0;
}

struct inode * ialloc(dev_t dev, mode_t mode, int * error)
{
	int i;
	ino_t ino;
	struct inode * tmp;
	struct super_block * sp;

	sp = sread(dev);

	/* must have been checked */
	if(sp->s_roflag){
		*error = -EROFS;
		return NULL;
	}

	ino = (ino_t)bitalloc(sp, IMAP);
	if((bit_t)ino == NOBIT){
		printk("no spare inode on dev major %d minor %d",
			MAJOR(dev), MINOR(dev));
		*error = -ENOSPC;
		return NULL;
	}

	tmp = iget(NODEV, ino);
	if(!tmp){
		ifree(sp, (bit_t)ino);
		*error = -ENOMEM;
		return NULL;
	}

	tmp->i_dev = dev;
	tmp->i_num = ino;

	tmp->i_ndzones = NR_DZONES;
	tmp->i_nindirs = NR_INDIRECTS;

	/* put into hash table */
	put_ihash(tmp);

	tmp->i_mode = mode;
	tmp->i_nlinks = 1;
	tmp->i_uid = current->euid;
       	tmp->i_gid = current->egid;
	tmp->i_size = 0;
	for(i = 0; i < NR_ZONES; i++)
		tmp->i_zone[i] = 0; /* NOZONE */

	tmp->i_dirty = 1;

	tmp->i_sp = sp;
	tmp->i_update = (ATIME | MTIME | CTIME);

	return tmp;
}

int get_iiucount(dev_t dev)
{
	int i, count;
	struct inode * inoptr;

	count = 0;
	for(i = 0, inoptr = &inodes[0]; i < NR_INODE; i++, inoptr++)
		if((inoptr->i_count > 0) && (inoptr->i_dev == dev))
			count++;

	return count;
}
