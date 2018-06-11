#include <asm/system.h>
#include <cnix/types.h>
#include <cnix/kernel.h>

static unsigned long b_mask;
static unsigned long b_active;
static int b_inflag;
static bottomproc_t b_actions[32];

void do_with_bottom(void)
{
	int i;
	unsigned long active;
	
	cli();
	if(!b_active || nested_intnum || b_inflag){
		sti();
		
		return;
	}
	b_inflag = 1;
	sti();

	do{
		cli();
		active = b_active & (~b_mask);
		b_active &= b_mask;
		sti();
			
		/* 
		 * b_mask is for process, not for interrupt, so needn't
		 * disable interrupt
		 */
		for(i = 0; i < END_B; i++)
			if(active & 1 << i){
				if(b_actions[i])
					b_actions[i]();
			}
	}while(b_active & (~b_mask));

	cli();
	b_inflag = 0;
	sti();
}

void raise_bottom(int idx)
{
	long flags;
	
	if(!b_actions[idx]){
		printk("idx %d bottom not set\n", idx);
		return;
	}

	lock(flags);
	b_active |= 1 << idx;
	unlock(flags);
}

void lock_bottom(int idx, unsigned long * flags)
{
	*flags = b_mask;
	b_mask |= 1 << idx;
}

void unlock_bottom(int idx, unsigned long flags)
{
	b_mask = flags;
	//b_mask &= ~(1 << idx);
}

void lock_all_bottom(unsigned long * flags)
{
	*flags = b_mask;
	b_mask = ~0;
}

void unlock_all_bottom(unsigned long flags)
{
	b_mask = flags;
}

void save_bottom_mask(unsigned long * flags)
{
	*flags = b_mask;
}

void restore_bottom_mask(unsigned long flags)
{
	b_mask = flags;
}

void set_bottom(int idx, bottomproc_t func)
{
	long flags;
	
	lock(flags);
	b_actions[idx] = func;
	unlock(flags);
}

void unset_bottom(int idx)
{
	long flags;
	
	lock(flags);
	b_actions[idx] = NULL;
	b_active &= ~(1 << idx);
	unlock(flags);
}

void init_bottom(void)
{
	b_mask = 0;
	b_active = 0;
	b_inflag = 0;
}
