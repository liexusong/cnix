#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/const.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>

int kp_dir_size;

// copy kernel page tables
void copy_kernel_pts(unsigned long from, unsigned long to)
{
	int i;
	unsigned long * from_dir, * to_dir;

	from_dir = (unsigned long *)from;
	to_dir = (unsigned long *)to;

	for(i = 0; i < KP_DIR_ENTNUM; i++)
		to_dir[i] = from_dir[i];
}

BOOLEAN copy_mmap_pts(
	struct mmap_struct * mmap,
	unsigned long from,
	unsigned long to
	)
{
	int i, j, m, n;
	unsigned long begin, end, addr;
	unsigned long * from_dir, * to_dir;
	unsigned long * from_pg_table, * to_pg_table, this_pg, that_pg;

	begin = mmap->start;
	end = mmap->end;

	if(!begin || (begin < USER_ADDR))
		return TRUE;

	from_dir = (unsigned long *)from;
	to_dir = (unsigned long *)to;

	for(addr = begin; addr < end;){
		i = page_dir_idx(addr);
		from_pg_table = (unsigned long *)(from_dir[i] & PAGE_MASK);

		if(!to_dir[i]){
			if(!(to_pg_table = (unsigned long *)get_one_page())){
				printk("Out of memory\n");
				return FALSE;
			}

			to_dir[i] = (unsigned long)to_pg_table | 7;
		}else
			to_pg_table = (unsigned long *)(to_dir[i] & PAGE_MASK);

		m = to_pt_offset(addr);

		if(end >= ((i + 1) << 22))
			n = 1024;
		else{
			n = to_pt_offset(end);
			if(end & ~PAGE_MASK)
				n += 1;
		}

		for(j = m; j < n; j++){
			this_pg = from_pg_table[j];
			if(!(this_pg & 1))
				continue;

			if(to_pg_table[j])
				DIE("page table item already exists\n"); 

			if(mmap->flags & MAP_SHARED){
				to_pg_table[j] = this_pg;
				mem_map[MAP_NR(this_pg)].count++;
				continue;
			}

			/* TODO: copy on write */
#if 0
			this_pg &= ~2;
			from_pg_table[j] = this_pg;
			to_pg_table[j] = this_pg;
#endif

			if(!(that_pg = get_one_page())){
				printk("out of memory\n");
				return FALSE;
			}

			memcpy(
				(void *)that_pg,
				(void *)(this_pg & PAGE_MASK),
				PAGE_SIZE
			);

			to_pg_table[j] = that_pg | (this_pg & 7);
#if 0
			mem_map[MAP_NR(this_pg)].count++;
#endif
		}

		addr = (i + 1) << 22;
	}

	return TRUE;
}

void free_mmap_pts(struct mmap_struct * mmap, unsigned long from)
{
	int i, j, m, n, rest;
	unsigned long begin, end, addr;
	unsigned long * from_dir;
	unsigned long * from_pg_table;

	begin = mmap->start;
	end = mmap->end;

	if(!begin || (begin < USER_ADDR))
		return;

	from_dir = (unsigned long *)from;

	for(addr = begin; addr < end;){
		i = page_dir_idx(addr);
		from_pg_table = (unsigned long *)(from_dir[i] & PAGE_MASK);
		if(!from_pg_table)
			DIE("BUG: cannot happen\n");

		m = to_pt_offset(addr);

		if(end >= ((i + 1) << 22))
			n = 1024;
		else{
			n = to_pt_offset(end);
			if(end & ~PAGE_MASK)
				n += 1;
		}

		rest = 0;

		for(j = 0; j < 1024; j++){
			if((j < m) || (j >= n)){
				if(from_pg_table[j])
					rest = 1;

				continue;
			}

			if(!(from_pg_table[j] & 1))
				DIE("BUG: cannot happen\n");

			free_one_page(from_pg_table[j] & PAGE_MASK);
			from_pg_table[j] = 0;
		}

		if(!rest){
			free_one_page((unsigned long)from_pg_table);
			from_dir[i] = 0;
		}

		addr = (i + 1) << 22;
	}
}

int free_page_tables(unsigned long from, int first, int size)
{
	int i, j;
	unsigned long * pg_dir, * pg_table;

	if(from & 0xfff)
		DIE("address not alignment\n");
	
	if(from == (unsigned long)kp_dir)
		DIE("trying to free kernel page dir\n");
	
	pg_dir = (unsigned long *)from;

	for(i = first; size > 0; size--, i++){
		if(!((pg_dir[i]) & 1))
			continue;

		/* kernel page dir item */
		if(i < KP_DIR_ENTNUM){
			pg_dir[i] = 0;
			continue;
		}

		pg_table = (unsigned long *)(pg_dir[i] & PAGE_MASK);

		for(j = 0; j < 1024; j++){
			if(pg_table[j] & 1)
				free_one_page(pg_table[j] & PAGE_MASK);

			pg_table[j] = 0;
		}
		
		free_one_page(pg_dir[i] & PAGE_MASK);
		pg_dir[i] = 0;
	}		

	return 0;
}

#define PAGE_EXIST	1
#define TO_VIR_ADDR(pde, pte)	((pde << 22) | (pte << 12))

typedef unsigned long phy_addr_t;
typedef unsigned long virt_addr_t;

#define PHY2VIRT(x)	(x)
#define EIGHT_8M_ADDR	(8 * 1024 * 1024)

virt_addr_t phy2virt(phy_addr_t addr)
{
	printk("phyical addr = %x\n", addr);
	if (addr <= EIGHT_8M_ADDR)
	{
		return (addr & PAGE_MASK);
	}
	else
		return (addr & PAGE_MASK);;
}

phy_addr_t virt2phy(virt_addr_t addr)
{
	printk("virtual addr = %x\n", addr);
	
	if (addr <= EIGHT_8M_ADDR)
	{
		return (addr & PAGE_MASK);
	}
	else
		return (addr & PAGE_MASK);
}

void print_page_dir(unsigned long pde_addr)
{
	virt_addr_t *dir;
	virt_addr_t *pte;
	phy_addr_t tmp;
	phy_addr_t pte_tmp;
	int i;
	int j;
	
	dir = (virt_addr_t *)pde_addr;
	if ((unsigned long)dir & (PAGE_SIZE - 1))
	{
		DIE("address is not aligned.\n");
	}

	for (i = 0; i < 1024; i++)
	{
		tmp = dir[i];
		if (!(tmp & PAGE_EXIST))
		{
			continue;
		}

		pte_tmp = tmp;

		pte = (virt_addr_t *)phy2virt(tmp);
		for (j = 0; j < 1024; j++)
		{
			tmp = pte[j];

			if (!(tmp & PAGE_EXIST))
			{
				continue;
			}
				
			printk("pde no: %d, pte no: %d, va: %x, pa: %x, pde flg:0x%x, pte flg: %x\n", 
				i, j, 
				TO_VIR_ADDR(i, j),
				(tmp & PAGE_MASK), 
				(pte_tmp & (PAGE_SIZE - 1)),
				tmp & (PAGE_SIZE - 1));
		}
	}
}
