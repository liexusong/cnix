#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/const.h>
#include <cnix/errno.h>
#include <cnix/string.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>

extern struct task_struct * task[NR_TASKS];
extern void ret_from_syscall(void);

int find_empty_process(void)
{
	int i, j;
	static int next_pid = 1;
	
	for(i = next_pid, j = 0; j < NR_TASKS; j++, i++){
		if(!task[i]){
			next_pid = (i % (NR_TASKS - 1)) + 1;
			return i;
		}

		if(i == (NR_TASKS - 1))
			i = 1;
	}

	return -EAGAIN;
}

int copy_process(
	int nr, struct regs_t * regs, void (*thread_func)(void *), void * data
	)
{
	int i;
	struct filp * fp;
	unsigned long dir_pg;
	struct regs_t * ptr;
	struct task_struct * p;
	unsigned long flags;

	p = (struct task_struct *)get_one_page();
	if(!p)
		return -EAGAIN;

	memcpy(p, current, sizeof(struct task_struct) + 128);

	dir_pg = (unsigned long)get_one_page();
	if(!dir_pg){
		free_one_page((unsigned long) p);	
		return -EAGAIN;	
	}

	p->pg_dir = dir_pg;

	copy_kernel_pts(current->pg_dir, p->pg_dir);

	for(i = 0; i < current->mm.mmap_num; i++){
		if(!copy_mmap_pts(&current->mm.mmap[i],
			current->pg_dir, p->pg_dir)){
			free_page_tables(p->pg_dir, KP_DIR_ENTNUM, USER_DIR_ENTNUM);
			free_one_page(dir_pg);
			free_one_page((unsigned long) p);
			return -ENOMEM;
		}
	}

	// XXX
	for(i = 0; i < current->mm.mmap_num; i++)
		if(current->mm.mmap[i].ino)
			current->mm.mmap[i].ino->i_count++;

	p->start_time = nowticks;
	p->stime = p->utime = 0;
	p->cstime = p->cutime = 0;

	/* new process discard alarm sig */
	p->alarm.expire = 0;
	list_init(&p->alarm.list);

	if(p->pwd)
		p->pwd->i_count++;

	if(p->root)
		p->root->i_count++;

	for(i = 0; i < OPEN_MAX; i++){
		fp = current->file[i];
		if(fp)
			fuse(fp);
	}

	p->pid = nr;
	p->ppid = current->pid;
	list_head_init(&p->clist);
	list_add_head(&current->clist, &p->plist);

	/* pgid, tty copied from father process */

	/* initilize it before pglist */
	p->sleep_spot = NULL;

	lockb_kbd(flags);
	list_add_head(&current->pglist, &p->pglist);
	unlockb_kbd(flags);
	
	p->esp0 = (unsigned long)p + PAGE_SIZE;	

	if(thread_func){
		/* 256 - 4 bytes will be used for kernel_exit */
		p->esp = (unsigned long)p + PAGE_SIZE - 256;
		*((unsigned long *)(p->esp + 4)) = (unsigned long)data;

		p->eip = (unsigned long)thread_func;
		p->kernel = 1;
	}else{
		if(!regs)
			DIE("BUG: cannot happen\n");

		if((unsigned long)regs->esp < USER_ADDR)
			DIE("BUG: cannot happen\n");

		p->esp = (unsigned long)p + PAGE_SIZE - sizeof(struct regs_t);

		p->eip = (unsigned long)ret_from_syscall;
		p->kernel = 0;

		ptr = (struct regs_t *)p->esp;

		*ptr = *regs;	
		ptr->eax = 0;	
	}	

	/* clear signal, or the system will trap into dead lock */
	p->signal = 0;

	task[nr] = p;

	/* keyboard, timer, ide, ... could wakeup sleeping process */
	lockb_all(flags);
	add_run(p);
	unlockb_all(flags);

	current->need_sched = 1;

	return 0;
}

int kernel_thread(void (*thread_func)(void *), void * data)
{
	int i, ret;

	i = find_empty_process();
	if(i < 0)
		return i;
	
	ret = copy_process(i, NULL, thread_func, data);
	if(ret < 0)
		return ret;

	return i;
}
