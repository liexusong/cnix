#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/head.h>
#include <cnix/kernel.h>

extern void divide_err(void);
extern void debug(void);
extern void nmi(void);
extern void int3(void);
extern void overflow(void);
extern void bounds(void);
extern void invalid_op(void);
extern void device_not_avl(void);
extern void double_fault(void);
extern void coprocessor_segment_overrun(void);
extern void invalid_tss(void);
extern void segment_not_present(void);
extern void stack_segment(void);
extern void general_protection(void);
extern void page_fault(void);
extern void coprocessor_fault(void);
extern void system_call(void);

extern int do_exit(int);

static void reserved(void)
{
	panic("error: idt is reserved.\n");
}

struct stack_frame 
{
	struct stack_frame *prev;
	unsigned long ret_address;
};

#define NR_CALL_DEPTH 5

// libc_start_main return address
#define TEXT_START_ADDRESS (current->mm.start_code)
#define END_LINK_ADDRESS (TEXT_START_ADDRESS + 0x1B)
#define KERNEL_START_ADDR	(0x100000)

static void dump_user_call_stack(unsigned long ebp)
{
	struct stack_frame *ptr;
	int i;

	printk("begin dump user stack:\n");
	ptr = (struct stack_frame *)ebp;
	for (i = 0; ptr && ((unsigned long)ptr != END_LINK_ADDRESS); i++)
	{
		if ((unsigned long)ptr <= TEXT_START_ADDRESS)
		{
			break;
		}
		
		printk("Stack <%d>: 0x%x \n", i, ptr->ret_address);

		ptr = ptr->prev;
	}
	printk("end dump user stack:\n");
}

#define IN_KERNEL_SPACE(ebp)	(((unsigned long)current < ebp) && (ebp < ((unsigned long)current + PAGE_SIZE)))
static void dump_stack(unsigned long ebp, unsigned long addr)
{
	unsigned char * eip;
	unsigned long * esp, offset;


	if (!IN_KERNEL_SPACE(ebp))
	{
		return;
	}

	// find the first call frame
	for (eip = (u8_t *)addr; (unsigned long)eip > KERNEL_START_ADDR; eip--)
	{
		if(EQ(*eip, 0x55) 
				&& EQ(*(eip + 1), 0x89) && EQ(*(eip + 2), 0xe5))
			break;
	}

	if ((unsigned long)eip == KERNEL_START_ADDR)
	{
		return;
	}

	offset = addr - (unsigned long)eip;
	printk("Stack: %x [%x + %x]\n", addr, eip, offset);
	
	while(((unsigned long)current < ebp)
		&& (ebp < ((unsigned long)current + PAGE_SIZE))){
		esp = (unsigned long *)ebp;
		for(eip = (u8_t *)(*(esp + 1)); ; eip--)
			if(EQ(*eip, 0x55) 
				&& EQ(*(eip + 1), 0x89) && EQ(*(eip + 2), 0xe5))
			break;
		offset = *(esp + 1) - (unsigned long)eip;
		printk("Stack: %x [%x + %x]\n", *(esp + 1), eip, offset);
		ebp = *esp;
	}

	//while(1);
}

void do_with_execption(struct regs_t regs)
{
	printk("\n");

	if(regs.es == 14)
		printk("page_fault, ");

	printk("current task pid %d execption %d err_code %x\n",
		current->pid, regs.es, regs.index);
	printk("cs: %x eip: %x eflags: %x ds: %x\n",
		regs.cs, regs.eip, regs.eflags, regs.ds);
	printk("eax: %x ebx: %x ecx: %x edx: %x ebp: %x\n",
		regs.eax, regs.ebx, regs.ecx, regs.edx,	regs.ebp);
	printk("esp: %x esi: %x edi: %x\n", regs.esp, regs.esi, regs.edi);
	printk("task name: %s\n", current->myname);
	if(ZERO(current->pid))
		while(1);

	dump_stack(regs.ebp, regs.eip);

	dump_user_call_stack(regs.ebp);
	
	do_exit(SIGSEGV);
	//sendsig(current, SIGSEGV);
}

void traps_init(void)
{
	int index;
	
	set_trap_gate(0, divide_err);
	set_trap_gate(1, debug);
	set_trap_gate(2, nmi);
	set_trap_gate(3, int3);
	set_trap_gate(4, overflow);
	set_trap_gate(5, bounds);
	set_trap_gate(6, invalid_op);
	set_trap_gate(7, device_not_avl);
	set_trap_gate(8, double_fault);
	set_trap_gate(9, coprocessor_segment_overrun);
	set_trap_gate(10, invalid_tss);
	set_trap_gate(11, segment_not_present);
	set_trap_gate(12, stack_segment);
	set_trap_gate(13, general_protection);
	set_trap_gate(14, page_fault);
	set_trap_gate(15, coprocessor_fault);

	for(index = 16; index < 32; index++)
		set_trap_gate(index, reserved);
	
	/* system call would auto cli */
	set_system_gate(0x80, &system_call);	
}
