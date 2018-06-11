#include <cnix/list.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>
#include <cnix/bottom.h>

struct page_descriptor {
	list_t list;
	struct block_header *pd_firstfree;
	int  pd_order;
	int  pd_nfrees;
};

#define PAGE_DESC_SIZE sizeof(struct page_descriptor)

struct block_header {
	unsigned long  bh_flags;
	struct block_header *bh_next;
};

struct sizes_descriptor {
	list_t list;
	struct page_descriptor *sd_dmafree;
	int     sd_nblocks;
	int 	sd_npages;	/* how many pages dose this bucket has */
	int	sd_nfrees;	/* how many free blocks in this bucket */
	unsigned long sd_gfporder;
};

/* the blocksize = (PAGE_SIZE - sizeof(page_descriptor))/number */
const unsigned int blocksize[] = {
	32,
	64,
	128,
	255,
	510,
	1020,
	2040,
	4096 - PAGE_DESC_SIZE,
	8192 - PAGE_DESC_SIZE,
	16384 - PAGE_DESC_SIZE,
	32768 - PAGE_DESC_SIZE,
	65536 - PAGE_DESC_SIZE,
	131072 - PAGE_DESC_SIZE,
	0
	};

struct sizes_descriptor sizes[] = {
	{{NULL, NULL}, NULL, 0, 0, 0, 0},
	{{NULL, NULL}, NULL, 0, 0, 0, 0},
	{{NULL, NULL}, NULL, 0, 0, 0, 0},
	{{NULL, NULL}, NULL, 0, 0, 0, 0},
	{{NULL, NULL}, NULL, 0, 0, 0, 0},
	{{NULL, NULL}, NULL, 0, 0, 0, 0},
	{{NULL, NULL}, NULL, 0, 0, 0, 0},
	{{NULL, NULL}, NULL, 0, 0, 0, 0},
	{{NULL, NULL}, NULL, 0, 0, 0, 1},
	{{NULL, NULL}, NULL, 0, 0, 0, 2},
	{{NULL, NULL}, NULL, 0, 0, 0, 3},
	{{NULL, NULL}, NULL, 0, 0, 0, 4},
	{{NULL, NULL}, NULL, 0, 0, 0, 5},
	{{NULL, NULL}, NULL, 0, 0, 0, 0},
};	

#define BLOCKSIZE(order)	(blocksize[order])
#define MFREE			0xAA55AA55UL

char * kmalloc(int size, int attr)
{
	struct page_descriptor *sd_firstfree, *pg;
	struct sizes_descriptor *bucket;
	struct block_header *bh;
	int order;
	char *p;
	unsigned long flags;

	bucket = sizes;
	order = 0;
	for(;;){
		if((size <= BLOCKSIZE(order)) || !BLOCKSIZE(order))
			break;
		bucket++;
		order++;
	}

	if(!BLOCKSIZE(order)){
		printk("The size you request is too large for cnix\n");
		return NULL;
	}

	lockb_all(flags);

	sd_firstfree = NULL;
	if(!list_empty(&bucket->list)){
		sd_firstfree = list_entry(
			bucket->list.next, list, struct page_descriptor
		);
	}

	/* has no free blocks, allocate */
	if((sd_firstfree == NULL)
		|| (sd_firstfree->pd_nfrees == 0)){
		unsigned long addr;
		struct block_header * tmp;
		int i;

		addr = get_free_pages(attr, bucket->sd_gfporder);
		if(!addr){
			unlockb_all(flags);

			printk("Not enough %d continous pages",
				bucket->sd_gfporder);
			return NULL;
		}

		/* initial the bucket and the page_descriptor */
		sd_firstfree = pg = (struct page_descriptor *)addr;

		list_add_head(&bucket->list, &pg->list);

		bucket->sd_npages++;
		pg->pd_nfrees = bucket->sd_nblocks;
		bucket->sd_nfrees += pg->pd_nfrees;
		pg->pd_order = order;

		/* PAGE_DESC_SIZE = sizeof(struct page_descriptor) */
		pg->pd_firstfree
			= (struct block_header *)(addr + PAGE_DESC_SIZE);

		addr += PAGE_DESC_SIZE;
		tmp = (struct block_header *)addr;

		/* add the free blocks to the list */
		for(i = 0;i < (bucket->sd_nblocks - 1);i++){
			addr += BLOCKSIZE(order); /* move to next block */

			tmp->bh_flags = MFREE;
			tmp->bh_next = (struct block_header *)(addr);
			tmp = tmp->bh_next;
		}

		/* the last one */
		tmp->bh_flags = MFREE;
		tmp->bh_next = NULL;
	}

	bh = sd_firstfree->pd_firstfree;
	if(bh->bh_flags != MFREE){
		unlockb_all(flags);

		printk("Error, block_header list in page is not free!\n");
		printk("this page pd_nfrees: %d \n", sd_firstfree->pd_nfrees);
		DIE("");

		return NULL;
	}

	sd_firstfree->pd_firstfree = bh->bh_next;
	sd_firstfree->pd_nfrees--;

	/* update the sizes_descriptor */
	bucket->sd_nfrees--;

	unlockb_all(flags);

	p = (char *)bh;

	return p;
}

#define PAGE_DESC(x) ((struct page_descriptor *)((unsigned long)x & PAGE_MASK))

void kfree(void *p)
{
	struct page_descriptor *pg;
	struct block_header *bh;
	struct sizes_descriptor *bucket;
	int order;
	unsigned long flags;

	pg =  PAGE_DESC(p);
	bh = (struct block_header *)p;

	lockb_all(flags);

	/* add the free block to the free list in page */
	bh->bh_next = pg->pd_firstfree;
	pg->pd_firstfree = bh;

	bh->bh_flags = MFREE;  /* mark it unused */

	order = pg->pd_order;
	bucket = &(sizes[order]);

	/* upate the page_desc and sizes_desc */
	pg->pd_nfrees++;
	bucket->sd_nfrees++;

	if(pg->pd_nfrees == bucket->sd_nblocks){ /* all free */
		list_del(&pg->list);

		/* update the size_desc */
		bucket->sd_npages--;
		bucket->sd_nfrees -= bucket->sd_nblocks;

		/* free the 2^sd_gfporder pages */
		__free_pages((unsigned long)pg,bucket->sd_gfporder);
	}else if(&pg->list != bucket->list.next){ // not the first one
		list_del(&pg->list);
		list_add_head(&bucket->list, &pg->list);
	}

	unlockb_all(flags);
}

#define SIZES_NUM (sizeof(sizes) / sizeof(struct sizes_descriptor))

void kmalloc_init(void)
{
	int i;

	for(i = 0; i < SIZES_NUM - 1; i++){
		int order = sizes[i].sd_gfporder;

		list_head_init(&sizes[i].list);
		sizes[i].sd_nblocks = (PAGE_SIZE << order) / blocksize[i];
	}
}
