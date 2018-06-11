#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/const.h>
#include <cnix/head.h>
#include <cnix/kernel.h>
#include <cnix/wait.h>
#include <cnix/signal.h>
#include <cnix/fs.h>
#include <cnix/string.h>

extern struct task_struct idle_task_struct;

struct{
	long * a;
	short b;
}stack_start = {(long *)((unsigned long)&idle_task_struct + PAGE_SIZE), 0x10};

struct tss_struct tss = {0, };

struct task_struct * current = &idle_task_struct;

/* take init_task.task as the head of run_queue */
list_t * run_queue = &idle_task_struct.list;

struct task_struct * task[NR_TASKS] = {&idle_task_struct, };

static void __sleepon(struct wait_queue **p, unsigned long flags);

void sched_init(void)
{
	int i;
	long flags;
	
	current->need_sched = 0;

	current->state = TASK_RUNNING;
	current->pg_dir = (unsigned long)kp_dir;

	current->esp0 = (unsigned long)&idle_task_struct + PAGE_SIZE;	

	current->priority = 10;
	current->counter = 10;

	memset(&current->mm, 0, sizeof(struct mm_struct));

	current->mm.start_code = (unsigned long)&_text;
	current->mm.end_code = (unsigned long)&_etext;
	current->mm.start_data = (unsigned long)&_data;
	current->mm.end_data = (unsigned long)&_edata;
	current->mm.start_stack = (unsigned long)&idle_task_struct;
	current->mm.end_stack = (unsigned long)&idle_task_struct + PAGE_SIZE;
	current->mm.start_bss = (unsigned long)&_bss;
	current->mm.end_bss = (unsigned long)&_ebss;

	current->mm.mmap_num = 4;
	
	current->start_time = nowticks;
	current->stime = current->utime = 0;
	current->cstime = current->cutime = 0;

	current->alarm.expire = 0;
	list_init(&current->alarm.list);

	current->signal = 0;
	current->blocked_saved = current->blocked = 0;
	for(i = 0; i < SIGNR; i++){
		current->sigaction[i].sa_handler = SIG_DFL;
		current->sigaction[i].sa_mask = 0;
		current->sigaction[i].sa_flags = 0;
	}

	current->uid = current->euid = current->suid = SU_UID;
	current->gid = current->egid = current->sgid = SU_GID;

	/* idle process has not working directory */
	current->pwd = current->root = NULL;

	for(i = 0; i < OPEN_MAX; i++){
		current->file[i] = NULL;
		current->fflags[i] = O_CLOEXEC;
	}

	current->umask = 0022;

	current->exit_code = 0;

	current->pid = IDLEPID;
	current->ppid = IDLEPID;
	list_head_init(&current->clist);
	list_head_init(&current->plist);

	current->pgid = IDLEPID;
	list_head_init(&current->pglist);

	current->tty = NOTTY; /* XXX */
	current->session = IDLEPID;

	current->sleep_spot = NULL;
	current->sleep_eip = 0;

	for(i = 0; i < RLIM_NLIMITS; i++){
		current->rlims[i].rlim_cur = RLIM_INFINITY;
		current->rlims[i].rlim_max = RLIM_INFINITY;
	}

	// RLIMIT_NPROC
	current->rlims[6].rlim_cur = NR_TASKS;
	current->rlims[6].rlim_max = NR_TASKS;

	// RLIMIT_NOFILE
	current->rlims[7].rlim_cur = OPEN_MAX;
	current->rlims[7].rlim_max = OPEN_MAX;

	current->check = 0;
	current->kernel = 1;

	strcpy(current->myname, "idle");

	tss.ss0 = 0x10;
	tss.esp0 = current->esp0;

	tss.iobase = (unsigned short)104;
	memset(tss.bitmap , 0xff, 128);
	tss.bitmap[128] = 0xff;

	set_tss_desc(gdt + TSS_ENTRY, &tss);
	ltr(TSS_ENTRY);

	list_head_init(run_queue);

	for(i = 1; i < NR_TASKS; i++)
		task[i] = NULL;

	lock(flags);
	nested_intnum--;
	unlock(flags);
}

/*
 * add into tail
 */
void add_run(struct task_struct * tsk)
{
	if(list_empty(run_queue))
		list_add_head(run_queue, &tsk->list);
	else
		list_add_tail(run_queue, &tsk->list);
}

/*
 * delete from run_queue
 */
void del_run(struct task_struct * tsk)
{
	list_del(&tsk->list);
}

BOOLEAN is_run_quene_empty(void)
{
	return list_empty(run_queue);
}

void idle_task(void)
{
	for (;;)
	{
		if (is_run_quene_empty())
		{
			__asm__ ("HLT\r\n"::);
		}
		else
		{
			schedule();
		}
	}
}

void schedule(void)
{
	int counter;
	list_t * tmp, * pos;
	struct task_struct * prev, * next, * p, * sel;
	unsigned long flags;

	lockb_all(flags); /* XXX */

	current->need_sched = 0;

	/* run_queue is empty */
	if(list_empty(run_queue)){
		/* no need to count idle process counter */

		prev = current;
		current	= list_entry(run_queue, list, struct task_struct);
		next = current;

		goto run_idle;
	}

repeat:
	counter = 0;
	sel = NULL;

	list_foreach(tmp, pos, run_queue){
		p = list_entry(tmp , list, struct task_struct);

		if(p->counter > counter){
			counter = p->counter;
			sel = p;
		}
	}
	
	if(counter == 0){
		list_foreach(tmp, pos, run_queue){
			p = list_entry(tmp, list, struct task_struct);
			p->counter = p->priority;
		}

		goto repeat;
	}

	prev = current;
	current = next = sel;

run_idle:
	unlockb_all(flags);

	if(prev == next)
		return;

	if(!prev->kernel)
		__asm__ __volatile__("fsave %0":"=m"(prev->i387)::"memory");

	// kernel process, needn't switch esp0 and page dir
	if(next->kernel)
		switch_to(prev, next, NULL);
	else{
		__asm__ __volatile__("frstor %0"::"m"(next->i387));

		memcpy(tss.bitmap, current->bitmap, 128);
		switch_to(prev, next, &tss);
	}
}

static void add_wait_queue(struct wait_queue ** p, struct wait_queue * wait) 
{
	wait->next = *p;
	*p = wait;
}

/*
 * remove wait from wait_queue *p
 */
static void remove_wait_queue(struct wait_queue ** p , struct wait_queue *wait)
{
	unsigned long flags;
	struct wait_queue * tmp;
	
	lockb_all(flags);
	
	if(*p == wait)
		*p = wait->next;
	else{
		tmp = *p;
		while(tmp && tmp->next != wait)
			tmp = tmp->next;
		
		if(!tmp)
			DIE("this cannot happen\n");

		tmp->next = wait->next;
	}

	wait->next = NULL;
	
	unlockb_all(flags);
}		

void sleepon(struct wait_queue ** p)
{
	{
		unsigned long retaddr;

		retaddr = (unsigned long)&p;
		retaddr -= 4;
		current->sleep_eip = *((unsigned long *)retaddr);
	}

	__sleepon(p, TASK_UNINTERRUPTIBLE);

	current->sleep_eip = 0;
}

void interrupt_sleepon(struct wait_queue ** p)
{
	/*
	 * this is not a good idea, but it works now, because p is a pointer.
	 * the same as do_with_sigal, need to be carefully, especially compile
	 * code when trying diffirent version gcc.
	 */

	{
		unsigned long retaddr;

		retaddr = (unsigned long)&p;
		retaddr -= 4;
		current->sleep_eip = *((unsigned long *)retaddr);
	}

	__sleepon(p, TASK_INTERRUPTIBLE);

	current->sleep_eip = 0;
}

static void __sleepon(struct wait_queue ** p, unsigned long state)
{
	unsigned long flags, bflags;
	struct wait_queue wait = { current, NULL };

	lockb_all(bflags);

	if(!p){
		unlockb_all(bflags);
		return;
	}

	if(current->pid == 0)
		DIE("process 0 cannot sleep\n");

	del_run(current);
	current->state = state;
	add_wait_queue(p, &wait);

	unlockb_all(bflags);

	saveb_flags(bflags);
	stib();

	save_flags(flags);	
	sti();

	schedule();

	remove_wait_queue(p, &wait);

	restore_flags(flags);
	restoreb_flags(bflags);
}

void wakeup(struct wait_queue ** p)
{
	unsigned long flags;
	struct wait_queue * tmp;
	struct task_struct * tsk;

	lockb_all(flags);
	
	if(!p || !(tmp = *p)){
		unlockb_all(flags);

		return;
	}

	do{
		tsk = tmp->task;

		if(tsk->state == TASK_UNINTERRUPTIBLE 
				|| tsk->state == TASK_INTERRUPTIBLE){
			tsk->state = TASK_RUNNING;
			tsk->sleep_spot = NULL;

			/* now tsk is still in wait queue */
			add_run(tsk);
		}

		tmp = tmp->next;
	}while(tmp);

	current->need_sched = 1;

	unlockb_all(flags);
}
