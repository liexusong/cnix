#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/timer.h>
#include <cnix/elf.h>
#include <cnix/fs.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>
#include <cnix/tty.h>
#include <cnix/unistd.h>

#define invalidate(pg_dir) __asm__("movl %%eax, %%cr3\n\t"::"a"(pg_dir))

#define ARGVOFF 4

// convert virtual address to physical address in user space
unsigned long convert_phy_addr(unsigned long pg_dir, unsigned long va)
{
	unsigned long tmp;

	tmp = ((unsigned long *)pg_dir)[page_dir_idx(va)];
	tmp &= PAGE_MASK;
	if (tmp == 0)
	{
		panic("can not happen1!\n");

		return 0;
	}

	tmp = ((unsigned long *)tmp)[to_pt_offset(va)];
	tmp &= PAGE_MASK;
	if (tmp == 0)
	{
		panic("can not happen2!\n");

		return 0;
	}

	tmp += va & (PAGE_SIZE - 1);
	return tmp;
}


// copy data from ptr to virtual address
unsigned long copy_data(char *ptr, unsigned long va, unsigned long pg_dir)
{
	int len;
	int page_left;
	unsigned long pa;
	int count;
	
	len = strlen(ptr) + 1;
	for (;;)
	{
		if (len == 0)
		{
			break;		// all are copyed.
		}
		
		page_left = PAGE_SIZE - (va & (PAGE_SIZE - 1));
		if (len > page_left)
		{
			count = page_left;
		}
		else
			count = len;

		if (!(pa = convert_phy_addr(pg_dir, va)))
		{
			return 0;
		}
		
		memcpy((void *)pa, ptr, count);

		len -= count;
		va  += count;
		ptr += count;
	}

	// align to int size
	va = (va + sizeof(int) - 1) & ~(sizeof(int) - 1);
	return va;
}

// argv && envp 
static int calc_param_length(char **argv, char **envp)
{
	int count = 1;
	int i;

	i = 0;
	if (argv)
	{
		for (; argv[i]; i++)
		{
			;
		}
	}

	count += i + 1;

	i = 0;
	if (envp)
	{
		for (; envp[i]; i++)
		{
			;
		}
	}

	count += i + 1;

	return count * sizeof(int);
}

static unsigned long copy_param(
	char ** argv,
	char ** envp,
	unsigned long stack,		// virtual address
	unsigned long pg_dir,
	int script)
{
	int i;
	unsigned long param_va;
	unsigned long param_pa;
	unsigned long data_va;
	unsigned long tmp;
	unsigned long pa;
	
	data_va = stack + calc_param_length(argv, envp);
	param_va = stack + ARGVOFF;

	// fill argv
	i = 0;
	if (argv)
	{
		for(; argv[i]; i++)
		{
			if (!(param_pa = convert_phy_addr(pg_dir, param_va)))
			{
				return 0;
			}
			
			if (i > 0 && !script && cklimit(argv[i]))
			{
				return 0;
			}
			
			tmp = copy_data(argv[i], data_va, pg_dir);

			// argv[i] = data;
			*(int *)param_pa = data_va;
			param_va += sizeof(int);
			data_va = tmp;

			//printk("argv[%d] = %s\n", i, argv[i]);
		}
	}

	if (!(param_pa = convert_phy_addr(pg_dir, param_va)))
	{
		return 0;
	}
	param_va += sizeof(int);
	*(int *)param_pa = 0;		// argv[argc] = NULL;

	// fill argc
	if (!(pa = convert_phy_addr(pg_dir, stack)))
	{
		return 0;
	}
	*(int *)pa = i; 
	
	if (envp)
	{
		for(i = 0; envp[i]; i++)
		{
			if (!(param_pa = convert_phy_addr(pg_dir, param_va)))
			{
				return 0;
			}

			if (script && cklimit(envp[i]))
			{
				return 0;
			}
			
			tmp = copy_data(envp[i], data_va, pg_dir);

			// argv[i] = data;
			*(int *)param_pa = data_va;
			param_va += sizeof(int);
			data_va = tmp;

			//printk("envp[%d] = %s\n", i, envp[i]);
		}
	}

	if (!(param_pa = convert_phy_addr(pg_dir, param_va)))
	{
		return 0;
	}
	param_va += sizeof(int);
	*(int *)param_pa = 0;		// envp[envp_count] = NULL;

	return param_va;
}

int fill_pgitem(unsigned long ** pg, unsigned long * pg_dir, unsigned long addr)
{
	*pg = (unsigned long *)(pg_dir[page_dir_idx(addr)] & (PAGE_MASK));
	if(!(*pg)){
		*pg = (unsigned long *)get_one_page();
		if(!(*pg))
			return -EAGAIN;

		pg_dir[page_dir_idx(addr)]= (unsigned long)(*pg) + 7;
	}

	return 0;
}

int load_elf(
	char * filename,
	int fd,
	int phnum,
	int phentsize,
	struct elf_phdr * phdr,
	unsigned long pg_dir,
	unsigned long * stkaddr,
	struct mm_struct * mm
	)
{
	int ret, i, prot, flags;
	unsigned long * pg, addr;

	ret = 0;
	pg = NULL;
	addr = USER_ADDR;

	for(i = 0; i < phnum; i++){
		if(phdr->p_type != PT_LOAD){
			phdr++;
			continue;
		}

		if(phdr->p_memsz == 0){
			phdr++;
			continue;
		}

		addr = phdr->p_vaddr;
		if((addr < USER_ADDR) || (phdr->p_memsz < phdr->p_filesz)){
			ret = -ENOEXEC;
			goto errout;
		}

		if(!mmap_check_addr(mm, addr, phdr->p_memsz)){
			PRINTK("mmap_check_addr failed\n");
			ret = -ENOMEM;
			goto errout;
		}

		prot = PROT_READ;

		if(phdr->p_flags == (PF_R | PF_X)){
			if(phdr->p_memsz != phdr->p_filesz){
				ret = -ENOEXEC;
				goto errout;
			}

			if(mm->start_code > 0){
				ret = -ENOEXEC;
				goto errout;
			}

			prot |= PROT_EXEC;
			flags = MAP_SHARED;
		}else{
			if(mm->start_data > 0){
				ret = -ENOEXEC;
				goto errout;
			}

			prot |= PROT_WRITE;
			flags = MAP_PRIVATE;
		}

		ret = do_mmap(
			addr,
			phdr->p_filesz,
			prot,
			flags,
			fd,
			phdr->p_offset,
			phdr->p_memsz,
			pg_dir,
			mm
			);
		if(MMAP_FAILED(ret))
			goto errout;

		phdr++;
	}

#define INIT_STACK_PAGES	32
#define PARAM_STACK_PAGES	5
#define STACKTOP		0xf0000000
#define STACKLOW \
	(STACKTOP - (PAGE_SIZE * (PARAM_STACK_PAGES + INIT_STACK_PAGES)))

	/* must have code segment */
	if((!mm->start_code) || (mm->end_code > STACKLOW)){
		ret = -ENOEXEC;
		goto errout;
	}

	if(!mm->start_data){
		mm->start_data = mm->start_code;
		mm->end_data = mm->start_code;
	}else if((mm->start_data < mm->end_code) || (mm->end_data > STACKLOW)){
		ret = -ENOEXEC;
		goto errout;
	}

	/*
	 * get INIT_STACK_PAGES + 1 pages for stack, including param_page,
	 * top-down from 0xf0000000.
	 */

	ret = do_mmap(
		STACKLOW,
		0,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		0,
		0,
		STACKTOP - STACKLOW,
		pg_dir,
		mm
		);
	if(MMAP_FAILED(ret))
		goto errout;

	*stkaddr = STACKTOP - (PAGE_SIZE * PARAM_STACK_PAGES);

	return 0;

errout:
	mmap_free_all(mm, pg_dir);

	return ret;
}

static int __do_execve(
	char * filename, char ** argv, char ** envp, struct regs_t * regs,
	uid_t suid, gid_t sgid,
	int script
	)
{
	int ret = 0, fd, i;
	unsigned long p, pg_dir, entry, stack;
	struct elfhdr * hdr;
	int phnum, phentsize;
	struct elf_phdr * phdr;
	struct mm_struct mm;
	struct inode * ino;

	p = get_one_page();
	if(!p)
		return -EAGAIN;

	/*
	 * TODO:
	 *   share code page
	 */

	ret = do_open(filename, O_RDONLY, 0);
	if(ret < 0)
		goto errout1;

	fd = ret;

	ino = fget(fd)->f_ino;

	ilock(ino);
	ino->i_count++;

	ret = do_read(fd, (char *)p, sizeof(struct elfhdr));
	if(ret != sizeof(struct elfhdr)){
		ret = -ENOEXEC;
		goto errout2;
	}

	hdr = (struct elfhdr *)p;

	if((hdr->e_ident[0] != '\177')
		|| (hdr->e_ident[1] != 'E')
		|| (hdr->e_ident[2] != 'L')
		|| (hdr->e_ident[3] != 'F')){
		ret = -ENOEXEC;
		goto errout2;
	}

	if(hdr->e_type != 2){
		ret = -ENOEXEC;
		goto errout2;
	}
	
	if(hdr->e_machine != 3){
		ret = -ENOEXEC;
		goto errout2;
	}

	/* page has been cleaned to zero */
	pg_dir = get_one_page();
	if(!pg_dir){
		ret = -EAGAIN;
		goto errout2;
	}

	phnum = hdr->e_phnum;
	phentsize = hdr->e_phentsize;
	entry = hdr->e_entry;

	if(!phnum || !phentsize){
		ret = -ENOEXEC;
		goto errout4;
	}

	ret = do_lseek(fd, hdr->e_phoff, SEEK_SET);
	if(ret != hdr->e_phoff)
		goto errout4;

	if(phentsize * phnum > PAGE_SIZE){
		printk("not support this elf file\n");
		ret = -ENOEXEC; /* XXX */
		goto errout4;
	}

	ret = do_read(fd, (char *)p, phentsize * phnum);
	if(ret != phentsize * phnum)
		goto errout4;

	memset(&mm, 0, sizeof(struct mm_struct));

	phdr = (struct elf_phdr *)p;

	ret = load_elf(
		filename,
		fd,
		phnum,
		phentsize,
		phdr,
		pg_dir,
		&stack,
		&mm
		);
	if(ret < 0)
		goto errout4;

	if (!copy_param(argv, envp, stack, pg_dir, script))
	{
		ret = -E2BIG;
		goto errout5;
	}

	iput(ino);

	do_close(fd);

	memcpy((void *)pg_dir, (void *)current->pg_dir, KP_DIR_ENTNUM * 4);

	// flush before invalidate
	mmap_flush_all(&current->mm, current->pg_dir);

	invalidate(pg_dir);

	mmap_free_all(&current->mm, current->pg_dir);
	memcpy(&current->mm, &mm, sizeof(struct mm_struct));

	free_one_page(current->pg_dir);
	current->pg_dir = pg_dir;

	for(i = 0; i < SIGNR; i++){
		current->sigaction[i].sa_handler = SIG_DFL;
		current->sigaction[i].sa_mask = 0;
		current->sigaction[i].sa_flags = 0;
	}

	for(i = 3; i < OPEN_MAX; i++)
		if(current->file[i] && (current->fflags[i] & O_CLOEXEC))
			do_close(i);

	current->suid = suid;
	current->sgid = sgid;

	strncpy(current->myname, filename, 128);

	__asm__ __volatile__("finit");

	rechk(); /* init/main.c will nochk to call execve */

	regs->eflags = 0x282;
	regs->cs = 0x1b;
	regs->eip = entry;
	regs->ds = 0x23;
	regs->es = 0x23;
	regs->ss = 0x23;
	regs->esp = stack;
	
	free_one_page(p);

	return 0;

errout5:
	mmap_free_all(&mm, pg_dir);
errout4:
	free_one_page(pg_dir);
errout2:
	iput(ino);
	do_close(fd);
errout1:
	free_one_page(p);

	return ret;
}

int do_execve(char * filename, char ** argv, char ** envp, struct regs_t * regs)
{
	int ret = 0, fd, i, error;
	unsigned long p;
	struct inode * inoptr;
	uid_t suid;
	gid_t sgid;
	char * exe_name = filename;
	char ** argv1 = argv;
	int script = 0;

	checkname(filename);

	inoptr = namei(filename, &error, 0);
	if(!inoptr)
		return error;

	if(!S_ISREG(inoptr->i_mode)){
		iput(inoptr);
		return -EISDIR;
	}

	if(!admit(inoptr, I_XB)){
		iput(inoptr);
		return -EACCES;
	}

	suid = inoptr->i_uid;
	sgid = inoptr->i_gid;

	iput(inoptr);

	p = get_one_page();
	if(!p)
		return -EAGAIN;

	ret = do_open(filename, O_RDONLY, 0);
	if(ret < 0){
		free_one_page(p);
		return ret;
	}

	fd = ret;

	ret = do_read(fd, (char *)p, 128);
	if(ret > 2){
		char * cp = (char *)p;

		if((cp[0] == '#') && (cp[1] == '!')){
			int ibak;

			i = 2;

			/* blanks between '#!' and '/bin/sh' */
			while((cp[i] == ' ') || (cp[i] == '\t')){
				i++;
				if(i >= ret)
					break;
			}

			ibak = i;

			/* look for '\r' or '\n' from head */
			for(; i < ret; i++)
				if((cp[i] == '\r') || (cp[i] == '\n')){
					cp[i] = '\0';
					break;
				}

			if(i > ibak){
				i--;

				/* look for ' ' or '\t' from tail */
				for(; i >= ibak; i--)
					if((cp[i] != ' ') && (cp[i] != '\t'))
						break;

				cp[i + 1] ='\0';

				if(i >= ibak){
					exe_name = &cp[ibak];

					argv1 = (char **)&cp[128];
					for(i = 1; i < 512; i++){
						argv1[i] = argv[i - 1];
						if(!argv1[i])
							break;
					}

					argv1[0] = exe_name;

					script = 1;
				}
			}
		}
	}

	do_close(fd);

	ret = __do_execve(
		exe_name, argv1, envp, regs, suid, sgid, script
		);

	free_one_page(p);

	return ret;
}

extern struct tty_struct ttys[TTY_NUM];

/*
 * after exit, its item in task array will be released by father
 */
int do_exit(int exitcode)
{
	unsigned long flags;
	int i, tty;
	asynctimer_t * async;
	struct task_struct * pp;

	if(current->pwd)
		iput(current->pwd);

	if(current->root)
		iput(current->root);

	for(i = 0; i < OPEN_MAX; i++)
		if(current->file[i])
			do_close(i);

	// before invalidate
	mmap_flush_all(&current->mm, current->pg_dir);

	invalidate((unsigned long)kp_dir);
	
	lockb_timer(flags);

	async = &current->alarm;
	if(async->expire != 0){
		async->expire = 0;
		list_del(&async->list);
	}

	unlockb_timer(flags);

	pp = task[current->ppid];

	/* change all child's father to init process */
	if(!list_empty(&current->clist)){
		list_t * tmp, * pos;
		struct task_struct * t = NULL;

		list_foreach(tmp, pos, &current->clist){
			t = list_entry(tmp, plist, struct task_struct);

			t->ppid = INITPID;
			list_del(tmp);
			list_add_tail(&task[INITPID]->clist, tmp);
		}

		if (t)
		{
			wakeup(task[INITPID]->sleep_spot);
		}
	}

	/* delete father-son relation in do_wait */

	lockb_all(flags);

	if(!(pp->sigaction[SIGCHLD - 1].sa_flags & SA_NOCLDSTOP))
		sendsig(pp, SIGCHLD);

	tty = current->tty;
	if(tty != NOTTY)
		tty -= 1;

	if(!list_empty(&current->pglist)){
		pid_t pgid;
		list_t * tmp, * pos;
		struct task_struct * t, * t1;

		tmp = current->pglist.next;
		t = list_entry(tmp, pglist, struct task_struct);

		/* group leader may be not the owner of tty */
		if((tty != NOTTY) && (current->pid == ttys[tty].t_owner)){
			ttys[tty].t_owner = 0;

			list_foreach(tmp, pos, &current->pglist){
				t1 = list_entry(tmp,
					pglist, struct task_struct);
				t1->tty = NOTTY;
			}
		}

		if(current->pid == current->pgid){
			pgid = t->pid;

			list_foreach(tmp, pos, &current->pglist){
				t = list_entry(tmp, pglist, struct task_struct);
				t->pgid = pgid;
			}
		}
	
		list_del(&current->pglist);
	}else{
		/*
		 * once process becomes the owner of tty, it will always be
		 * the owner, until this process ends it's life.
		 */
		if((tty != NOTTY) && (current->pid == ttys[tty].t_owner))
			ttys[tty].t_owner = 0;
	}

	del_run(current);

	unlockb_all(flags);

	current->state = TASK_ZOMBIE;
	current->exit_code = exitcode;	

	mmap_free_all(&current->mm, current->pg_dir);

	free_one_page(current->pg_dir);

	if(pp->exit_code > 0)
		wakeup(pp->sleep_spot);

	schedule();

	return 0;
}

#define __NR_kernel_exit __NR_exit

_syscall1(int, kernel_exit, int, exitcode)

static int wait_for_child(void)
{
	unsigned long flags;
	struct wait_queue * wait = NULL;

	current->exit_code = 1; // being used when child exits

	current->sleep_spot = &wait;
	sleepon(&wait);

	current->exit_code = 0;

	lockb_all(flags);
	if(current->signal){
		if(!(current->signal & (1 << (SIGCHLD - 1)))){
			unlockb_all(flags);
			return -EINTR;
		}
	}
	unlockb_all(flags);

	return 0;
}

int do_wait(int * status, int option)
{
	pid_t pid;
	list_t * tmp, * pos;
	struct task_struct * t = NULL;

	if(list_empty(&current->clist))
		return -ECHILD;

check_again:
	list_foreach(tmp, pos, &current->clist){
		t = list_entry(tmp, plist, struct task_struct);

		if(t->state == TASK_ZOMBIE)
			break;
	}

	if(t->state != TASK_ZOMBIE){
		unsigned long flags;
		struct wait_queue * wait = NULL;
		if (option == WNOHANG)
		{
			return 0;
		}

		current->exit_code = 1; // being used when child exits

		current->sleep_spot = &wait;
		sleepon(&wait);

		current->exit_code = 0;

		lockb_all(flags);
		if(current->signal){
			if(!(current->signal & (1 << (SIGCHLD - 1)))){
				unlockb_all(flags);
				return -EINTR;
			}
		}
		unlockb_all(flags);

		goto check_again;
	}

	list_del(&t->plist); /* delete form father's child list */

	if (status)
	{
		*status = t->exit_code;
	}

	pid = t->pid;

	task[pid] = NULL;
	free_one_page((unsigned long)t);

	return pid;
}

int do_waitpid(pid_t pid, int *status, int option)
{
	pid_t grp_id;
	list_t *pos;
	list_t *tmp;
	struct task_struct *t, *t1;

	if (list_empty(&current->clist))
	{
		return -ECHILD;
	}

	if (option != 0 && option != WNOHANG && option != WUNTRACED)
	{
		return -EINVAL;
	}

do_again:
	t1 = NULL;
	if (pid == -1)
	{
		return do_wait(status, option);
	}
	else if (pid > 0)
	{
		list_foreach(tmp, pos, &current->clist)
		{
			t = list_entry(tmp, plist, struct task_struct);

			if (t->pid == pid)
			{
				t1 = t;
				break;
			}
		}
	}
	else
	{
		if(pid == 0)
			grp_id = current->pgid;
		else
			grp_id = -pid;

		list_foreach(tmp, pos, &current->clist)
		{
			t = list_entry(tmp, plist, struct task_struct);
			if (t->pgid != grp_id)
				continue;

			t1 = t;
			if (t->state == TASK_ZOMBIE)
			{
				break;
			}
		}
	}

	if (!t1)
	{
		return -ECHILD;
	}

	if (t1->state != TASK_ZOMBIE && option == WNOHANG)
	{
		return 0;
	}
	
	if (t1->state == TASK_ZOMBIE)
	{
		list_del(&t1->plist); /* delete form father's child list */

		if (status)
		{
			*status = t1->exit_code;
		}

		pid = t1->pid;

		task[pid] = NULL;
		free_one_page((unsigned long)t1);

		return pid;
	}
	else if (t1->state == TASK_STOPPED && option == WUNTRACED)
	{
	/*
		Todo.
		if (not_report_last_stop_state(t))
		{
		
		}
	*/
	}
	else
	{
		if (wait_for_child() < 0)
		{
			return -EINTR;
		}
		
		goto do_again;	
	}

	// NOT REACHED.
	return pid;
}

int do_brk(unsigned long end_data_segment)
{
	int brked;
	unsigned long addr, p, * pg;
	struct mm_struct * mm = &current->mm;
	struct mmap_struct mmap;

	/* end_data_segment must be less than (unsigned long)(-4096) */
	if(end_data_segment > mmap_low_addr(mm))
		return -ENOMEM;

	if(!end_data_segment)
		return mm->end_data;

	/* code must be under data */
	if(end_data_segment < mm->start_data)
		return -EINVAL;

	/* has paged */
	if(mm->end_data >= end_data_segment)
	{
		mmap.start = PAGE_ALIGN(end_data_segment);
		mmap.end = mm->end_data;

		free_mmap_pts(&mmap, current->pg_dir);

		mm->end_data = end_data_segment;
		return 0;
	}

	if(end_data_segment <= PAGE_ALIGN(mm->end_data)){
		mm->end_data = end_data_segment;
		return 0;
	}

	addr = mm->end_data;

	for(; addr < end_data_segment; addr += brked){
		if(fill_pgitem(&pg,
			(unsigned long *)current->pg_dir, addr) < 0){
			mmap.start = PAGE_ALIGN(mm->end_data);
			mmap.end = addr;
			free_mmap_pts(&mmap, current->pg_dir);
			return -ENOMEM;
		}

		p = pg[to_pt_offset(addr)];
		if(!p){
			p = get_one_page();

			if(!p){
				mmap.start = PAGE_ALIGN(mm->end_data);
				mmap.end = addr;
				free_mmap_pts(&mmap, current->pg_dir);
				return -ENOMEM;
			}

			pg[to_pt_offset(addr)] = p + 7;
		}
		else
		{
			char *ptr;
			int len;

			ptr = (char *)((unsigned long)p & PAGE_MASK);
			ptr += (addr & (PAGE_SIZE - 1));
			len = PAGE_SIZE - (addr & (PAGE_SIZE - 1));

			if (len > end_data_segment - addr)
			{
				len = end_data_segment - addr;
			}
			
			memset((void *)ptr, 0, len);
		}

		brked = PAGE_SIZE - (addr & (PAGE_SIZE - 1));
		if(brked > end_data_segment - addr)
			brked = end_data_segment - addr;
	}

	mm->end_data = end_data_segment;

	return 0;
}
