#ifndef _KERNEL_H
#define _KERNEL_H

#include <cnix/limits.h>
#include <cnix/types.h>
#include <cnix/sched.h>
#include <cnix/bottom.h>
#include <cnix/list.h>
#include <asm/regs.h>

/*
 * 0 is ok, or false, comparing with PAGE_SIZE is for somegthing as SIG_IGN ...
 */
#define	cklimit(param) \
	(current->check \
		&& ((unsigned long)param >= PAGE_SIZE) \
		&& (((unsigned long)(param)) < USER_ADDR))

/* 0 is ok, or false */
#define ckoverflow(param, size) \
	((current->check) \
	&& (param) \
	&& (((unsigned long)(param)) + (size) < ((unsigned long)(param)))) 

#define nochk() (current->check = 0)
#define rechk() (current->check = 1)

#define checkname(name) \
	if(!name || (name[0] == '\0')) \
		DIE("cannot happen\n");

/* used after ck... */
#define sys_checkname(name) \
	if(name[0] == '\0') \
		return - EINVAL;

extern char * getname(const char *);
extern void freename(char *);

extern int nested_intnum;

extern struct tss_struct tss;	
extern union task_union init_task;
extern list_t * run_queue;
extern struct task_struct * task[NR_TASKS];

extern void add_run(struct task_struct *);
extern void del_run(struct task_struct *);
extern void sched_init(void);
extern void schedule(void);
extern void sleepon(struct wait_queue **);
extern void wakeup(struct wait_queue **);
extern int kernel_thread(void (*)(void *), void *);
extern int kernel_exit(int);

extern BOOLEAN mmap_check_addr(
	struct mm_struct * mm, unsigned long addr, unsigned long memsz
	);

#define mmap_find_space(mm, length) \
	__mmap_find_space(mm, 0xc0000000, length)

extern unsigned long mmap_low_addr(struct mm_struct * mm);

extern int printk(const char * fmt, ...);
extern int sprintf(char * buff, const char * fmt, ...);
extern void panic(const char * fmt, ...);

#define DIE(fmt, args...) \
do{ \
	printk("FUNCTION: %s in file %s, line %d\nMESSAGE: "fmt, \
		__FUNCTION__, __FILE__, __LINE__, ##args); \
	(*((unsigned char *)0)) = 0; \
}while(0)

#define PRINTK(fmt, args...) \
	printk("FUNCTION: %s in file %s, line %d\n"fmt, \
		__FUNCTION__, __FILE__, __LINE__, ##args);
#define PANIC(fmt, args...) \
	panic("FUNCTION: %s in file %s, line %d\n"fmt, \
		__FUNCTION__, __FILE__, __LINE__, ##args);

typedef void (*irqhandler_t)(void);

extern void intr_init(void);
extern int put_irq_handler(int, void (*)(struct regs_t *));
extern void disable_irq(int);
extern void enable_irq(int);
extern BOOLEAN anysig(struct task_struct *);
extern void sendsig(struct task_struct *, int);

extern int fill_pgitem(
	unsigned long ** pg, unsigned long * pg_dir, unsigned long addr
	);

extern int  do_mmap(
	unsigned long start,
	size_t length,
	int prot,
	int flags,
	int fd,
	off_t offset,
	size_t memsz,
	unsigned long pg_dir,
	struct mm_struct * mm
	);
extern int do_munmap(unsigned long start, size_t length);
extern int do_msync(unsigned long start, size_t length, int flags);
extern void mmap_free_all(struct mm_struct * mm, unsigned long pg_dir);
extern void mmap_flush_all(struct mm_struct * mm, unsigned long pg_dir);

#endif
