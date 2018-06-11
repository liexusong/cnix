#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/head.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>
#include <cnix/wait.h>

#define NR_FREE_LIST 10

#define invalidate() \
__asm__("movl %%eax, %%cr3"::"a"((unsigned long)kp_dir))

int nfreepages;
int ncodepages, ndatapages, nreservedpages, nmemmap_and_bitmap;

mem_map_t * mem_map; 

static struct wait_queue * pages_wait_queue;

static unsigned long free_area_init(unsigned long, unsigned long);

void __free_pages(unsigned long, int);
void free_one_page(unsigned long);
unsigned long get_free_pages(unsigned long, int);

/*
 * setup physical memory, start_mem is the end of data, end_mem is the last
 * byte of the total memory 
 */
unsigned long paging_init(unsigned long start_mem, unsigned long end_mem)
{

	int index, i;
	unsigned long addr, * pg_table, * pg_dir;

	pages_wait_queue = NULL;

	pg_dir = (unsigned long *)kp_dir;
	start_mem = PAGE_ALIGN(start_mem);
	addr = 0x800000; /* the first 8M is initialized in head.S */

	index = 2;

	while(addr < end_mem){
		pg_dir[index++] = start_mem | 3; /* present, r/w, s */

		pg_table = (unsigned long *)start_mem;
		for(i = 0; (i < FRAME_PER_PAGE) && (addr < end_mem); i++){
			pg_table[i] = addr | 3;
			addr += PAGE_SIZE;
		}
		start_mem += PAGE_SIZE;
	}

	kp_dir_size = index;
#if 0
	printk("The kp_dir_size is %d\n", kp_dir_size);
	if(addr != end_mem)
		printk("error: addr: %x != end_mem\n", addr);
#endif

	for(; index < 1024; index++)
		pg_dir[index] = 0;

	/* not mapped, for NULL */
	((unsigned long *)(pg_dir[0] & PAGE_MASK))[0] = 0;

	invalidate();

	return free_area_init(start_mem, end_mem);
}
	
struct free_area {
		struct page *prev;
		struct page *next;
		int * map;     /* the bitmap of memory's buddy system */
		int count;
};

struct free_area free_area_list[NR_FREE_LIST];

#define LONG_ALIGN(x)	((x) + sizeof(long) - 1)&~((sizeof(long)) - 1)

static unsigned long free_area_init(
	unsigned long start_mem,
	unsigned long end_mem
	)
{
	mem_map_t * p;
	int map_nr, order;
	struct free_area * area;

	mem_map = (mem_map_t *)start_mem;
	map_nr =  MAP_NR(end_mem);   /* the total of page frame */
	p = mem_map + map_nr;
	memset((char *)(mem_map), 0, (long)(p) - (long)mem_map);
	start_mem = LONG_ALIGN((unsigned long)p); 

	do{
		--p;
		p->flags = (1 << PageReserved) | (1 << PageDMA);
		p->count = 0;
	}while(p > mem_map);

	/* 
	* now start_mem will point to the address of bitmap
	*/
	for(area = free_area_list, order = 0;
		area < &(free_area_list[NR_FREE_LIST]); area++, order++){
		int bitlength;

		bitlength = end_mem >> (PAGE_SHIFT + order + 1);
		bitlength = (bitlength + 7) >> 3;
#if 0
		printk("order: %d bitlength: %d \n",order,bitlength);
#endif		
		area->map =(int *)start_mem;
		memset((char *)(area->map), 0, bitlength);
		area->next = area->prev = (struct page *)area;
		start_mem += bitlength;
		start_mem = LONG_ALIGN(start_mem);
	}
		
	printk("There are %d pages \n", map_nr);

	return start_mem;
}

unsigned long get_one_page()
{
	unsigned long addr;

	if(!(addr = get_free_pages(0, 0)))
		return 0;

	return addr;
}

/*
 * order is the power of 2
 */
unsigned long get_free_pages(unsigned long attr, int order)
{
	unsigned long flags;
	int new_order, index;
	unsigned long size, map_nr, addr;
	struct free_area *area;
	struct page *ret;

	if (order >= NR_FREE_LIST)
	{
		DIE("BUG: cannot happen\n");
	}

	lockb_all(flags);

try_again:
	area = &(free_area_list[order]);
	new_order = order;

	do{	
#if 1
		ret = area->next;
#else
		ret = area->prev;
#endif
		/*
		 * free_area[order] has free page to allocate
		 */
		if(ret != (struct page *)area)
			break;

		area++;
		new_order++;
	} while(new_order < NR_FREE_LIST);
		
	/* 
	 * now we don't implement page swap
	 */
	if(new_order == NR_FREE_LIST){
		if(attr & PageWait){
			sleepon(&pages_wait_queue);
			goto try_again;
		}

		unlockb_all(flags);

		printk ("Not Enough Free Pages, Giving up .....\n");

		return 0;
	}

	size = 1 << new_order;

	area -> next = ret -> next;
	ret -> next -> prev = (struct page *)area;

	map_nr = ret - mem_map;

	/* mark it that this block is being used */
	index = map_nr >> (new_order + 1);
	area->map[index / (sizeof(int) * 8)]
		^= 1 << (index % (sizeof(int) * 8));

	/* 
	 * if we must take page from  much bigger block
	 */
	while(new_order > order){
		area--;
		new_order--;
		size >>= 1;

		/* insert into more smaller list */
		ret->next = area->next;
		ret->prev = (struct page *)area;
		ret->next->prev = ret;
		area->next = ret;

		area->count++;

		map_nr = ret - mem_map;
		index = map_nr >> (new_order + 1);
		area->map[index / (sizeof(int) * 8)]
			^= 1 << (index % (sizeof(int) * 8));

		ret += size; /* take the back half of the block */
		map_nr += size;
	}

	ret->count = 1;
	nfreepages -= 1 << order;
		
	unlockb_all(flags);

	addr = map_nr << PAGE_SHIFT;

	memset((char *)addr, 0, (PAGE_SIZE * (1 << order)));

	return (map_nr << PAGE_SHIFT);
}

void free_one_page(unsigned long addr)
{
	struct page *pg;
	unsigned long map_nr, flags;
		
	map_nr = MAP_NR(_pa(addr));
	pg  = &(mem_map[map_nr]);
	/* if could dispaly the track of be called will be better */
	if(pg->count <= 0)
		DIE("Trying to free FREE page %x ...", addr);

	/* reserved page can not be freed */
	if(pg->flags & (1 << PageReserved))
		return;
	
	/* avoid to be called in interrupt handler again */
	lockb_all(flags);
	--pg->count;
	if(pg->count == 0){
		unlockb_all(flags);
		__free_pages(addr, 0);
	}else
		unlockb_all(flags);
}

/*
 * The size which is to be freed is 2**order 
 */
void __free_pages(unsigned long addr, int order)
{
	unsigned long flags;
	struct page *prev,*next;
	struct free_area *area;
	unsigned long map_nr,mask;
	unsigned long index;

	mask = ~((1UL << order) - 1);
	map_nr = MAP_NR(_pa(addr));
	map_nr &= mask;
	area = &(free_area_list[order]);
	index = map_nr >> (order + 1);

	lockb_all(flags);

	while(mask + (1 << (NR_FREE_LIST - 1))){
		/* the buddy sytem is not free */
		area->map[index / (sizeof(int) * 8)]
			^= 1 << (index % (sizeof(int) * 8));

		if((area->map[index / (sizeof(int) * 8)])
			& (1 << (index % (sizeof(int) * 8))))
			break;

		/* remove the buddy system from the free_area_list*/
		prev = mem_map[map_nr ^ -mask].prev; /* get buddy page point*/
		next = mem_map[map_nr ^ -mask].next;
		prev->next = next;
		next->prev = prev;

		area->count--;

		/* check the large block*/
		mask <<= 1;
		map_nr &= mask;

		index >>= 1;
		area++;
	}
				
	/* insert into correct pos */
	mem_map[map_nr].next = area->next;
	mem_map[map_nr].prev = (struct page *)area;

	area->next->prev = &(mem_map[map_nr]);
	area->next = &(mem_map[map_nr]);

	area->count++;

	nfreepages += 1 << order;
	
	unlockb_all(flags);

	wakeup(&pages_wait_queue);
}

void show_area()
{
	struct free_area *area;
	struct page *pg;
	int order;

	for (area = free_area_list, order = 0; 
		area < &(free_area_list[NR_FREE_LIST]);area++,order++){
		printk("area order: %d ,area count: %d\n", order, area->count);

		pg = area->prev;
		if(pg != (struct page *)area)
			printk("The first addr: 0x%x\n", 
				((pg - mem_map) << PAGE_SHIFT));
	}
}
