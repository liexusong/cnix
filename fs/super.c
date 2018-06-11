#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>
#include <cnix/errno.h>

#define NR_SB		10
#define BOOT_BLOCK	0
#define SUPER_BLOCK	1
#define SUPER_SIZE	((int)(&(((struct super_block *)(0))->s_rooti)))

#define u32size 	(sizeof(unsigned long))
#define u32bits 	(u32size * 8)	
#define BITS_PER_BLOCK	(BLOCKSIZ * 8)

#define INODES_PER_BLOCK (BLOCKSIZ / INODE_SIZE)

#define LM_MAGIC	0x2478	/* V2, 30 char names */

static struct super_block sb[NR_SB];

void init_super(void)
{
	int i;
	
	for(i = 0; i < NR_SB; i++){
		memset(&sb[i], 0, sizeof(struct super_block));
		sb[i].s_dev = NODEV;
		sb[i].s_count = 0;
	}
}

bit_t bitalloc(struct super_block * sp, int which)
{
	int i, j;
	unsigned long * tmp;
	block_t from_block, bit_blocks, block;
	bit_t bit, max_bit, from_bit;
	struct buf_head * bh;

	if(!sp)
		DIE("BUG: sp can't be NULL\n");

	if(sp->s_dev == NODEV)
		DIE("BUG: allocate bit from invalid device\n");
	
	if(sp->s_roflag)
		return NOBIT;

loop:
	/* lock super block */
	if(sp->s_flags & LOCK_BITOP){
		sp->s_flags |= WANT_BITOP;
		sleepon(&sp->s_bitop_wait);

		goto loop;
	}

	sp->s_flags |= LOCK_BITOP;

	if(which == IMAP){
		max_bit = sp->s_ninodes;
		from_block = SUPER_BLOCK + 1;
		from_bit = sp->s_isearch;
		bit_blocks = sp->s_imblocks;
	}else{
		max_bit = sp->s_zones - sp->s_fdzone;
		from_block = SUPER_BLOCK + 1 + sp->s_imblocks;
		from_bit = sp->s_zsearch;
		bit_blocks = sp->s_zmblocks;
	}

	/* run round */
	if(from_bit > max_bit)
		from_bit = 1; // reserve zero bit

	bit = NOBIT;

	block = (from_bit / BITS_PER_BLOCK);
	while(block < bit_blocks){
		bh = bread(sp->s_dev, from_block + block);
		if(!bh){
			printk("error when read bitmap\n");
			goto out;
		}

		tmp = (unsigned long *)bh->b_data;
		for(i = 0; i < (BLOCKSIZ / u32size); i++, tmp++)
			if(*tmp < 0xffffffff)
				break;
			
		if(i >= (BLOCKSIZ / u32size)){
			block++;
			brelse(bh);

			continue;
		}
		
		for(j = 0; (1 << j) & *tmp; j++);

		bit = block * BITS_PER_BLOCK + i * u32bits + j;

		if(bit > max_bit){
			brelse(bh);

			bit = NOBIT;
			goto out;
		}
	
		*tmp |= 1 << j;

#if 0
		bh->b_flags |= B_ASY;
		bwrite(bh);
#else
		bh->b_flags |= B_DIRTY | B_DONE;
		brelse(bh);
#endif

		if(which == IMAP)
			sp->s_isearch = bit + 1;
		else{
			sp->s_zsearch = bit + 1;
			bit += (sp->s_fdzone - 1); /* XXX */
		}

		break;
	}

out:
	/* unlock super block */
	sp->s_flags &= ~(LOCK_BITOP);
	if(sp->s_flags & WANT_BITOP){
		sp->s_flags &= ~WANT_BITOP;
		wakeup(&sp->s_bitop_wait);
	}

	return bit;
}

void bitfree(struct super_block * sp, bit_t bit, int which)
{
	bit_t max_bit;
	unsigned long * tmp;
	block_t from_block, block;
	struct buf_head * bh;

	if(!sp)
		DIE("BUG: sp can't be NULL\n");

	if(sp->s_dev == NODEV)
		DIE("BUG: free bit to invalid device\n");
	
	if(sp->s_roflag)
		return;

	if(which == IMAP){
		from_block = SUPER_BLOCK + 1;
		max_bit = sp->s_ninodes - 1;
	}else{
		from_block = SUPER_BLOCK + 1 + sp->s_imblocks;
		max_bit = sp->s_zones - sp->s_fdzone - 1;
		bit -= (sp->s_fdzone - 1);
	}

	if(bit > max_bit){
		printk("BUG want to free invalid bit!\n"); 

		return;
	}

loop:
	/* lock super block */
	if(sp->s_flags & LOCK_BITOP){
		sp->s_flags |= WANT_BITOP;
		sleepon(&sp->s_bitop_wait);

		goto loop;
	}

	sp->s_flags |= LOCK_BITOP;

	block = (bit / BITS_PER_BLOCK);	

	bh = bread(sp->s_dev, from_block + block);
	if(!bh){
		printk("read bitmap error\n");
		goto out;
	}	

	tmp = (unsigned long *)bh->b_data;

	bit %= BITS_PER_BLOCK;
	tmp += bit / u32bits;
	*tmp &= ~(1 << (bit % (u32bits)));
	
#if 0
		bh->b_flags |= B_ASY;
		bwrite(bh);
#else
		bh->b_flags |= B_DIRTY | B_DONE;
		brelse(bh);
#endif

	if(which == IMAP){
		if(bit < sp->s_isearch)
			sp->s_isearch = bit;
	}else{
		if(bit < sp->s_zsearch)
			sp->s_zsearch = bit;
	}

out:
	/* unlock super block */
	sp->s_flags &= ~(LOCK_BITOP);
	if(sp->s_flags & WANT_BITOP){
		sp->s_flags &= ~WANT_BITOP;
		wakeup(&sp->s_bitop_wait);
	}
}

void sfree(struct super_block * sp)
{
	sp->s_count--;
}

struct super_block * sread(dev_t dev)
{
	int i;
	struct buf_head * bh;
	struct super_block * tmp;

	tmp = NULL;
loop:
	for(i = 0; i < NR_SB; i++){
		if(sb[i].s_dev == dev){
			/*
			 * label loop can't be here, because everything
			 * could be change after wake up
			 */
			if(sb[i].s_flags & LOCK_S){
				sb[i].s_flags |= WANT_S;
				sleepon(&sb[i].s_wait);

				goto loop;		
			}
			
			return &sb[i];
		}else if(!tmp && (sb[i].s_count == 0))
			tmp = &sb[i];
	}

	/* this won't happen */
	if(!tmp){
		printk("super block is not enough!\n");
		return NULL;
	}

	tmp->s_rooti = NULL;
	tmp->s_mounti = NULL;

	tmp->s_count = 1;

	tmp->s_wait = NULL;
	tmp->s_bitop_wait = NULL;

	tmp->s_dev = dev;

	tmp->s_version = LM_MAGIC;
	tmp->s_ipblock = INODES_PER_BLOCK;
	tmp->s_ndzones = NR_DZONES;
	tmp->s_nindirs = NR_INDIRECTS;

	tmp->s_roflag = 0;	/* default is writable */

	tmp->s_isearch = 0;
	tmp->s_zsearch = 0;

	/* may sleep, so lock, read a new super block, so equal, not or */
	tmp->s_flags = LOCK_S;

	bh = bread(dev, SUPER_BLOCK);
	if(!bh){
		printk("error when reading super block\n");
		goto errout;
	}

	memcpy(tmp, bh->b_data, SUPER_SIZE);
	
	brelse(bh);

	/* check magic first */
	if(tmp->s_magic != LM_MAGIC){
		printk("magic error\n");
		goto errout;
	}

	/* basic check */
	if((tmp->s_imblocks < 1) || (tmp->s_zmblocks < 1)
		|| (tmp->s_ninodes < 1) || (tmp->s_zones < 1)
		|| (tmp->s_lzsize != 0)) /* XXX */
		goto errout;
	
	tmp->s_flags &= ~LOCK_S;

	/*
	 * someone wants this super block,
         * sleeping is a wonderful thing, it can change everything,
	 * but it's not necessary to check other bit,
	 * because super block hasn't been read into memory
	 */
	if(tmp->s_flags & WANT_S){
		tmp->s_flags &= ~WANT_S;
		wakeup(&tmp->s_wait);
	}

	return tmp;

errout:
	tmp->s_dev = NODEV;

	tmp->s_flags &= ~LOCK_S;
	if(tmp->s_flags & WANT_S){
		tmp->s_flags &= ~WANT_S;
		wakeup(&tmp->s_wait);
	}
	
	return NULL;
}

// get the super block of dev
struct super_block * get_sb(dev_t dev)
{
	int i;

	if (dev == NODEV)
	{
		return NULL;
	}

	for (i = 0; i < NR_SB; i++)
	{
		if (sb[i].s_dev == dev)
		{
			return &sb[i];
		}
	}

	return NULL;
}

static int get_zero_bits(unsigned long val, int bits)
{
	int count = 0;
	int i;

	if (bits > 32)
		bits = 32;

	for (i = 0; i < bits; i++)
	{
		if (!((val >> i) & 1))
		{
			count++;
		}
			
	}

	return count;
}

//
int get_free_bits_nr(struct super_block *sp, int *pcount, int which)
{
	int i;
	int bit;
	int max_bit_index;
	int from_block;
	int max_block;
	int block;
	int count;
	int error = 0;
	unsigned long *tmp;
	struct buf_head *bh;

loop:
	/* lock super block */
	if(sp->s_flags & LOCK_BITOP)
	{
		sp->s_flags |= WANT_BITOP;
		sleepon(&sp->s_bitop_wait);

		goto loop;
	}

	sp->s_flags |= LOCK_BITOP;

	if (which == ZMAP)
	{
		// s_zones is the total blocks
		// s_fdzone is the index of first data zone
		max_bit_index = sp->s_zones - sp->s_fdzone;
		from_block = SUPER_BLOCK + 1 + sp->s_imblocks;
		max_block = sp->s_zmblocks;
	}
	else
	{
		max_bit_index = sp->s_ninodes;
		from_block = SUPER_BLOCK + 1;
		max_block = sp->s_imblocks;
	}

#if 0
	printk("wich = %s, from_block = %d, max_block = %d, max_bit = %d\n",
			which == ZMAP ? "zmap" : "imap",
			from_block,
			max_block,
			max_bit);
#endif
	count = 0;
	bit = 0;
	block = 0;
	while (block < max_block)
	{
		bh = bread(sp->s_dev, from_block + block);
		if (!bh)
		{
			printk("error when read bitmap\n");
			error = -EIO;
			goto out;
		}

		tmp = (unsigned long *)bh->b_data;
		for (i = 0; i < (BLOCKSIZ / u32size); i++, tmp++)
		{
			int bit_num;
			
			if (bit > max_bit_index)
			{
				break;
			}
			
			if (bit + 32 > max_bit_index)
			{
				bit_num = max_bit_index - bit + 1;
			}
			else
			{
				bit_num = 32;
			}
			
			count += get_zero_bits(*tmp, bit_num);
			bit += bit_num;
		}
		
		block++;
		brelse(bh);

		if (bit > max_bit_index)
		{
			break;
		}
	}

out:
	/* unlock super block */
	sp->s_flags &= ~(LOCK_BITOP);
	if(sp->s_flags & WANT_BITOP)
	{
		sp->s_flags &= ~WANT_BITOP;
		wakeup(&sp->s_bitop_wait);
	}

	*pcount = count;

#if 0
	printk("count = %d\n", count);
#endif	
	return error;
}
