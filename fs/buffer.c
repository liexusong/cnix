#include <asm/system.h>
#include <cnix/fs.h>
#include <cnix/mm.h>
#include <cnix/driver.h>
#include <cnix/kernel.h>

/* The head of buffer free list */
static list_t bfreelist;
static list_t hash_table[NR_HASH];

/* The free list is empty, process must sleep and wait on wait queue */
static struct wait_queue * buf_wait;
static struct buf_head buf_head_array[NR_BUF];

static struct buf_head * getblk_from_hash(int, int);
static void putblk_to_hash(struct buf_head *);
static void notavail(struct buf_head *, int);
static void iowait(struct buf_head *);

struct buf_head * getblk(int, int);
void brelse(struct buf_head *);

/* 
 * TODO: setup a timer to flush buffer in some time interval
 *       flassh all when shutdown system
 */
void buf_init(void)
{
	int i;
	unsigned long addr;
	struct buf_head * bh;

	buf_wait = NULL;
	list_head_init(&bfreelist);

	addr = 0;
	for (i = 0; i < NR_BUF; i++){
		bh = &buf_head_array[i];

		bh->b_dev = NODEV;
		bh->b_blocknr = 0;
		bh->b_flags = 0;
		bh->b_count = 0;
		bh->b_wait = NULL;

		if(!(i % 4))
			addr = get_one_page();

		if(!addr)
			DIE("Can't get one page in buf_init");

		bh->b_data = (unsigned char *)(addr + (i % 4) * BLOCKSIZ);

		list_head_init(&bh->b_rel);
		list_add_head(&bfreelist, &bh->b_rel_free);
	}

	for (i = 0; i < NR_HASH; i++)
		list_head_init(&hash_table[i]);

	printk("Buffer number : %d, Hash number : %d\n", NR_BUF, NR_HASH);
}

static int ll_rw_block(int rw_mode, struct buf_head * buf)
{
	return ide_rw_block(rw_mode, buf);
}

/*
 * return a busy buffer with data
 */
struct buf_head * bread(int dev, int block)
{
	struct buf_head * bh;

	bh = getblk(dev, block);

	if(bh->b_flags & B_DONE)
		return bh;

	/* read from disk */
	if(ll_rw_block(READ, bh))
		iowait(bh);

	/* once error, always ERROR */
	if(bh->b_flags & B_ERROR){
		brelse(bh);

		return(NULL);
	}

	return bh;
}

/*
 * return a LOCKED buf
 */
struct buf_head * getblk(int dev, int block)
{
	unsigned long flags;
	struct buf_head * bh;

	lockb_ide(flags);

loop:	
	/* the buf is in hash table */
	if((bh = getblk_from_hash(dev, block))){ 
		if(bh->b_flags & B_BUSY){
			/* mark it, another process wants it */
			bh->b_flags |= B_WANTED;
			bh->b_count++;
			sleepon(&bh->b_wait);
			bh->b_count--;

			goto loop;
		}

		/* prevent others form using it */
		notavail(bh, 0);

		unlockb_ide(flags);

		return(bh);
	}

	/* no free buffer, just wait */
	if(list_empty(&bfreelist)){
		sleepon(&buf_wait);
		goto loop;
	}

	/* remove buffer from free list and hash talbe queue */
	bh = list_entry(bfreelist.next, b_rel_free, struct buf_head);
	notavail(bh, 1);

	/* no buffer is free and clean to use */
	if(bh->b_flags & B_DIRTY){ 
		if(bh->b_dev == NODEV)
			DIE("BUG: bh->b_dev can't be NODEV\n");

		/* async write */
		bh->b_flags |= B_ASY;
		bh->b_flags &= ~B_DIRTY;
		notavail(bh, 0);
		
		unlockb_ide(flags);

		bwrite(bh);

		lockb_ide(flags);

		goto loop;
	}

	bh->b_dev = dev;
	bh->b_blocknr = block;
	bh->b_flags = (B_BUSY | B_READ);

	/* cannot set b_wait to NULL, or next waiting process will be removed */
	/*bh->b_wait = NULL;*/

	/* put buffer to hash */
	putblk_to_hash(bh);

	unlockb_ide(flags);

	return bh;
}

/*
 * write data to disk, asy or syn, delay write, bh is in busy state
 * write directly method, set B_ASY, call bwrite
 * write directly and sync method, don't set B_ASY, call bwrite
 */
void bwrite(struct buf_head * bh)
{
	long flags;

	flags = bh->b_flags;

	/* buffer, marked B_DIRTY, written back to disk in getblk */
	bh->b_flags &= ~(B_READ | B_ERROR | B_DIRTY);
	ll_rw_block(WRITE, bh);

	/* sync write, we must wait */
	if((flags & B_ASY) == 0){
		iowait(bh);
		brelse(bh);
	}
}

/* 
 * wait for an i/o completion, bh is in busy state,
 * any error should be cared in the calling function, like bread or bwrite
 */
static void iowait(struct buf_head *bh)
{
	unsigned long flags;

	lockb_ide(flags);
	
	/* if current is waken up, B_DONE must be set except getting signal */
	while(!(bh->b_flags & B_DONE))
		sleepon(&bh->b_wait);

	unlockb_ide(flags);
}

/*
 * free a B_BUSY buf, add it to free list tail
 * delay write method: set B_DIRTY and B_DONE, call brelse
 */
void brelse(struct buf_head * bh)
{
	unsigned long flags;

	if(!bh)
		return;

	if(!(bh->b_flags & B_BUSY)){
		printk("free a free buffer again!\n");
		return;
	}

	/* some processes is waiting for this buffer */
	if(bh->b_flags & B_WANTED)
		wakeup(&bh->b_wait);

	/* some processes is waiting for freelist */
	if(buf_wait)
		wakeup(&buf_wait);

	lockb_ide(flags);

	/* not clean B_DIRTY */
	bh->b_flags &= ~(B_WANTED | B_BUSY | B_ASY);

	/* put bh to the tail of free list */
	list_add_tail(&bfreelist, &bh->b_rel_free);

	unlockb_ide(flags);
}

#define hashfn(dev, block) (&hash_table[((block) ^ (dev)) % NR_HASH])

/*
 * who call getblk_from_hash need to disable interrupt
 */
static struct buf_head * getblk_from_hash(int dev, int blocknr)
{
	list_t * hash, * tmp, * pos;
	struct buf_head * bh;

	hash = hashfn(dev, blocknr);
	list_foreach(tmp, pos, hash){
		bh = list_entry(tmp, b_rel, struct buf_head);
		if((bh->b_dev == dev) && (bh->b_blocknr == blocknr))
			return bh;
	}

	return NULL;
}

/*
 * bh may have been in hash_table
 * who call putblk_to_hash need to disable interrupt
 */
static void putblk_to_hash(struct buf_head * bh)
{
	list_t * hash;

	hash = hashfn(bh->b_dev, bh->b_blocknr);
	list_add_head(hash, &bh->b_rel);
}

/*
 * flag: 0, only del from free list
 *       1, del from both free list and hast table
 * who call notavail need to disable interrupt
 */
static void notavail(struct buf_head * bh, int flag)
{
	bh->b_flags |= B_BUSY;

	/* XXX: if flag is 1, b_dev is NODEV, then what happen */
	if(flag && bh->b_dev != NODEV)
		list_del1(&bh->b_rel);

	list_del1(&bh->b_rel_free);
}

/* 
 * hard disk interrupt call this function to tell that an i/o operation 
 * has been competed
 */
void iodone(struct buf_head *bh)
{
	unsigned long flags;

	lockb_ide(flags);

	bh->b_flags |= B_DONE;

	if(bh->b_flags & B_ASY)
		brelse(bh);
	else if(bh->b_flags & B_READ){
		bh->b_flags &= ~B_READ;
		wakeup(&bh->b_wait);
	}

	unlockb_ide(flags);
}

/*
 * cnix is not preempt kernel, needn't lock hash_table.
 */
void bsync(void)
{
	int i, again;
	unsigned long flags;
	list_t * hash, * tmp, * pos;
	struct buf_head * bh;

check_again:
	again = 0;

	for(i = 0; i < NR_HASH; i++){
		hash = &hash_table[i];

		list_foreach(tmp, pos, hash){
			bh = list_entry(tmp, b_rel, struct buf_head);

			lockb_ide(flags);
loop:
			if(bh->b_flags & B_DIRTY){
				if(bh->b_flags & B_BUSY){
					/* mark it, another process wants it */
					bh->b_flags |= B_WANTED;
					bh->b_count++;
					sleepon(&bh->b_wait);
					bh->b_count--;
					//printk("busy\n");
					goto loop;
				}
				
				notavail(bh, 0);
				unlockb_ide(flags);

				bwrite(bh);

				again = 1;
				continue;
			}

			unlockb_ide(flags);
		}
	}

	if(again)
		goto check_again;
}

void inval_dev(dev_t dev)
{
	unsigned long flags;
	list_t * tmp, * pos;
	struct buf_head * bh;

	lockb_ide(flags);

	/*
	 * buffer in free list, unlike sync and inval_block, needn't to check
	 * busy bit. and don't care if there is anyone who wants this block
	 * or not, just invalidate it.
	 */
	list_foreach(tmp, pos, &bfreelist){
		bh = list_entry(tmp, b_rel, struct buf_head);
		if(bh->b_dev == dev){
			bh->b_dev = NODEV;
			bh->b_blocknr = 0;
			bh->b_flags = 0;
			list_del1(&bh->b_rel);
		}
	}

	unlockb_ide(flags);
}

void inval_block(dev_t dev, block_t block)
{
	unsigned long flags;
	struct buf_head * bh;

	lockb_ide(flags);

loop:	
	/* the buf is in hash table */
	if((bh = getblk_from_hash(dev, block))){ 
		/*
		 * busy, or current process is not the last one who wants this
		 * block.
		 */
		if((bh->b_flags & B_BUSY) || (bh->b_count > 0)){
			/* mark it, another process wants it */
			bh->b_flags |= B_WANTED;
			bh->b_count++;
			sleepon(&bh->b_wait);
			bh->b_count--;

			goto loop;
		}

		bh->b_dev = NODEV;
		bh->b_blocknr = 0;
		bh->b_flags = 0;

		if(bh->b_wait){
			unlockb_ide(flags);
			DIE("BUG: cannot happen\n");
		}

		list_del1(&bh->b_rel);
		if(list_empty(&bh->b_rel_free))
			list_add_head(&bfreelist, &bh->b_rel_free);

		unlockb_ide(flags);
	}
}
