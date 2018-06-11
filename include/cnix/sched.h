#ifndef _SCHED_H
#define _SCHED_H

#include <cnix/const.h>
#include <cnix/signal.h>
#include <cnix/types.h>
#include <cnix/head.h>
#include <cnix/wait.h>
#include <cnix/timer.h>
#include <cnix/file.h>
#include <cnix/limits.h>
#include <cnix/list.h>
#include <asm/regs.h>
#include <cnix/tty.h>
#include <cnix/resource.h>

#define TASK_RUNNING		0
#define TASK_INTERRUPTIBLE	1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE		3
#define TASK_STOPPED		4

#define TSS_ENTRY		5

struct i387_struct{
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];
};

struct tss_struct{
	long	linkage;
	long	esp0;
	long	ss0;
	long	esp1;
	long	ss1;
	long	esp2;
	long	ss2;
	long	cr3;
	long	eip;
	long	eflags;
	long	eax;
	long	ecx;
	long	edx;
	long	ebx;
	long	esp;
	long	ebp;
	long	esi;
	long	edi;
	long	es;
	long	cs;
	long	ss;
	long	ds;
	long	fs;
	long	gs;
	long	ldt;
	unsigned short trace, iobase;
	unsigned char bitmap[128 + 4];
};

#define MAX_MMAP_NUM 16

#define PROT_READ	0x1
#define PROT_WRITE	0x2
#define PROT_EXEC	0x4
#define PROT_NONE	0x0
#define PROT_GROWSDOWN	0x01000000 /* mprotect only */
#define PROT_GROWSUP	0x02000000 /* mprotect only */

#define MAP_SHARED	0x01	/* share changes */
#define MAP_PRIVATE	0x02	/* changes are private */
#define MAP_TYPE	0x0f	/* mask for type of mapping */

#define MAP_FIXED	0x10	/* interpret addr exactly */
#define MAP_FILE	0
#define MAP_ANONYMOUS	0x20	/* don't use a file */
#define MAP_ANON	MAP_ANONYMOUS

/* flags to `msync' */
#define MS_ASYNC	1	/* sync memory asynchronously */
#define MS_SYNC		4	/* synchronous memory sync */
#define MS_INVALIDATE	2	/* invalidate the caches */

struct mmap_struct{
	unsigned long start, end;
	int flags;
	int prot;
	struct inode * ino;
	off_t offset;
	size_t length;
};

struct mm_struct{
	int mmap_num;
	struct mmap_struct mmap[MAX_MMAP_NUM];
};

// don't abuse below macros
#define start_code	mmap[0].start
#define end_code	mmap[0].end
#define start_data	mmap[1].start
#define end_data	mmap[1].end
#define start_stack	mmap[2].start
#define end_stack	mmap[2].end
#define start_bss	mmap[3].start
#define end_bss		mmap[3].end

#define MMAP_FAILED(x) (((int)(x) < 0) && ((int)(x) > -4096))

struct task_struct{
	int need_sched;
	unsigned long signal;
	unsigned long esp;
	unsigned long eip;
	unsigned long esp0;
	unsigned long pg_dir;	

	sigset_t blocked;
	sigset_t blocked_saved;
	struct sigaction sigaction[SIGNR];

	long state;
	
	list_t list;	/* run list or sleep list */

	long priority;
	long counter; 

	struct mm_struct mm;

	unsigned long start_time;
	unsigned long stime, utime, cstime, cutime;

	asynctimer_t alarm;
	
	uid_t uid, euid, suid;
	gid_t gid, egid, sgid;

	struct inode * pwd, * root;

	struct filp * file[OPEN_MAX];
	int fflags[OPEN_MAX];

	mode_t umask;
	
	int exit_code;

	pid_t pid, ppid;
	list_t clist;	/* child list */
	list_t plist;	/* linked in parent's child list */

	pid_t pgid;	/* which process group */
	list_t pglist;	/* process group list */

#define NOTTY (TTY_NUM + 1) /* no controlling tty */

	int tty;
	int session;	

	struct wait_queue ** sleep_spot;

	struct rlimit rlims[RLIM_NLIMITS];
#if 1
	unsigned long sleep_eip;
#endif
	char check;	/* check addr limit */
	char kernel;
	
	struct i387_struct i387;

#if 0
	unsigned short used_math;
	struct desc_struct ldt[2];
#endif

	unsigned char bitmap[128];

	char myname[0];	/* the last item in struct task_struct */
};

extern char __task_struct_dummy[2048 - sizeof(struct task_struct)];

#define ltr(n) __asm__("ltr %%ax"::"a"((unsigned long)n << 3))

void switch_to(struct task_struct *, struct task_struct *, struct tss_struct *);

extern struct task_struct idle_task_struct;
extern struct task_struct * current;
extern struct task_struct * task[NR_TASKS];

#define IDLEPID 0
#define INITPID 1

/* make sure task_struct is not too large */
extern char dummy [2048 - sizeof(struct task_struct)];

#endif
