#ifndef _REGS_H
#define _REGS_H

struct regs_t{
	long	ebx;
	long	ecx;
	long	edx;
	long	esi;
	long	edi;
	long	ebp;
	long	eax;
	long	ds;
	long	es;	/* index of execption */
	long	index;	/* index of interrupt, error of exception */
	long	eip;
	long	cs;
	long	eflags;
	long	esp;
	long	ss;
};
	
#endif
