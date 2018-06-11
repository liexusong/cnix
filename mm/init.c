#include <cnix/head.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>

/*
 * get the physical memory info
 */
void get_mem(unsigned long * start_memp, unsigned long * end_memp)
{
	unsigned long end_mem;
	unsigned long tmp;

	end_mem = 1UL << 20;
	tmp = (*(unsigned short *)(0x7c00 + 2)); /* extend memory */
	end_mem = (end_mem + (tmp << 10)) & 0xfffff000;
	*end_memp = end_mem;
	*start_memp = (unsigned long)&_end;
#if 0
	printk("02 value 0x%x\n",((*(unsigned  short *)(0x7c00 + 2))));
	printk("end_mem 0x%x\n",end_mem);
#endif	
}

extern void kmalloc_init(void);

/* 
 * add all free pages to free_area_list 
 * and caculate the Reserved page number
 */
void mem_init(unsigned long start_mem, unsigned long end_mem)
{
	unsigned long addr;
	struct page * pg;

	nfreepages = 0;
	ncodepages = ndatapages = nreservedpages = nmemmap_and_bitmap = 0;

	addr = RESERVED_MEM;
	while(addr < LOW_MEM){
		pg = &mem_map[(addr >> PAGE_SHIFT)];

		/* address < 16M , is the region of dma using */
		if(addr > DMA_HIGH_MEM) 
			pg->flags &= ~(1 << PageDMA);
		pg->flags &= ~( 1 << PageReserved);
		pg->count = 1;
		free_one_page(addr);
		nfreepages++;
		
		addr += PAGE_SIZE;
	}

	addr = 0;
	start_mem = PAGE_ALIGN(start_mem);
	end_mem = (end_mem) & PAGE_MASK;
	while(addr < start_mem){
		if((addr >= 0x100000) && (addr < (unsigned long)&_etext))
			ncodepages++;
		else if((addr >= (unsigned long)&_etext) 
			&& (addr < (unsigned long)&_end))
			ndatapages++;
		else if(addr >= (unsigned long)&_end)
			nmemmap_and_bitmap++;
		nreservedpages++;

		addr += PAGE_SIZE;
	}

	while(addr < end_mem){
		pg = &mem_map[(addr >> PAGE_SHIFT)];

		/* address less than 16M , can be used for dma */
		if(addr > DMA_HIGH_MEM) 
			pg->flags &= ~(1 << PageDMA);
		pg->flags &= ~(1 << PageReserved);
		pg->count = 1;
		free_one_page(addr);
		nfreepages++;
		addr += PAGE_SIZE;
	}

	kmalloc_init();

#if 0 	
	printk("page start at 0x%x\n", start_mem);
	printk("Reserved Pages: %d\n", nreservedpages);
	printk("Code  Pages: %d\n", ncodepages);
	printk("Data  Pages: %d\n", ndatapages);
	printk("mem_map and bitmap Pages: %d\n", nmemmap_and_bitmap);
	printk("Free  Pages:%d\n", nfreepages);
#endif	
}
