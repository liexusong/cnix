#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/unistd.h>
#include <cnix/config.h>
#include <cnix/errno.h>
#include <cnix/stdarg.h>
#include <cnix/driver.h>
#include <cnix/fs.h>
#include <cnix/timer.h>
#include <cnix/signal.h>
#include <cnix/kernel.h>
#include <cnix/string.h>
#include <cnix/time.h>
#include <cnix/times.h>
#include <cnix/resource.h>
#include <cnix/poll.h>

extern int find_empty_process(void);
extern int copy_process(int, struct regs_t *, void (*)(void *), void *);
extern int do_execve(char *, char **, char **, struct regs_t *);
extern int do_exit(int);
extern int do_waitpid(pid_t pid, int *status, int option);
extern int do_sigreturn(struct regs_t *);
extern int do_brk(unsigned long);
extern int do_select(int maxfdp1, fd_set *readset, fd_set *writeset,  fd_set *exceptset, struct timeval *timeout);
extern int do_poll(struct pollfd * fds, unsigned long nfds, int timeout);
extern int do_user_syslog(int priority, const char * buff, int len);

int sys_fork(struct regs_t regs)
{
	int i, ret;

	i = find_empty_process();
	if(i < 0)
		return i;
	
	ret = copy_process(i, &regs, NULL, NULL);
	if(ret < 0)
		return ret;

	return i;
}

int sys_execve(struct regs_t regs)
{
	int ret;
	char * tmp;
	char * filename = (char *)regs.ebx; 
	char ** argv = (char **)regs.ecx;
	char ** envp = (char **)regs.edx;

	if(cklimit(filename) || cklimit(argv) || cklimit(envp))
		return -EFAULT;

	sys_checkname(filename);

	tmp = getname(filename);
	if(!tmp)
		return -EAGAIN;

	ret = do_execve(tmp, (char **)regs.ecx, (char **)regs.edx, &regs);

	freename(tmp);

	return ret;
}

int sys_waitpid(pid_t pid, int * status, int option)
{
	int ret;

	if(cklimit(status) || ckoverflow(status, sizeof(int)))
		return -EFAULT;

	ret = do_waitpid(pid, status, option);

	return ret;
}

int sys_exit(int exitcode)
{
	int ret;

	ret = do_exit((exitcode & 0xff) << 8);

	return ret;
}

int sys_alarm(int seconds)
{
	unsigned long flags;
	unsigned long ticks;
	asynctimer_t * async;

	async = &current->alarm;

	lockb_timer(flags);

	ticks = async->expire;
	if(ticks != 0){
		list_del(&async->list);
		async->expire = 0;
	}

	unlockb_timer(flags);

	if(seconds){
		async->expire = seconds * HZ;
		asynctimer_set(async);
	}	

	return (ticks / HZ);
}

int sys_pause(void)
{
	struct wait_queue * wait = NULL;

	current->sleep_spot = &wait;
	sleepon(&wait);

	return -EINTR;
}

sighandler_t sys_signal(int signum, sighandler_t handler)
{
	sighandler_t oldhandler;

	if(signum < 1 || signum > SIGNR)
		return SIG_ERR;

	if(cklimit(handler))
		return SIG_ERR;

	/* needn't lock, because dealing with signal is under process context */
	oldhandler = current->sigaction[signum - 1].sa_handler;
	current->sigaction[signum - 1].sa_handler = handler;

	return oldhandler;
}

#define CANNOTBLOCKSIG ((1 << (SIGKILL - 1)) | (1 << (SIGSTOP - 1)))

int sys_sigaction(
	int signum,
	const struct sigaction * act,
	struct sigaction * oact
	)
{
	if(signum < 1 || signum > SIGNR)
		return -EINVAL;

	if(oact){
		if(cklimit(oact) || ckoverflow(oact, sizeof(struct sigaction)))
			return -EFAULT;

		*oact = current->sigaction[signum - 1];
	}

	if(act){
		if(cklimit(act))
			return -EFAULT;
		
		if(cklimit(act->sa_handler))
			return -EFAULT;

		current->sigaction[signum - 1] = *act;
		current->sigaction[signum - 1].sa_mask &= ~CANNOTBLOCKSIG;
	}

	return 0;
}

int sys_sigprocmask(int how, const sigset_t * set, sigset_t * oset)
{
	if(oset){
		if(cklimit(oset) || ckoverflow(oset, sizeof(sigset_t)))
			return -EFAULT;

		*oset = current->blocked;
	}

	if(set){
		if(cklimit(set) || ckoverflow(set, sizeof(sigset_t)))
			return -EFAULT;

		switch(how){
		case SIG_BLOCK:
			current->blocked |= (*set);
			break;
		case SIG_UNBLOCK:
			current->blocked &= ~(*set);
			break;
		case SIG_SETMASK:
			current->blocked = *set;
			break;
		default:
			return -EINVAL;
		}

		current->blocked &= ~CANNOTBLOCKSIG;
	}

	return 0;
}

int sys_sigpending(sigset_t * set)
{
	if(cklimit(set) || ckoverflow(set, sizeof(sigset_t)))
		return -EFAULT;

	*set = current->signal;

	return 0;
}

/*
 * carefully, sigsuspend must return after signal handler. could next
 * lines of code assure this point?
 * sys_sigsuspend->sleep->get a signal->sys_sigsuspend return->before return
 * to user state, do_with_signal->signal handler->return to user state.
 */
int sys_sigsuspend(const sigset_t * sigmask)
{
	struct wait_queue * wait = NULL;

	if (!sigmask)
	{
		return -EFAULT;
	}
	if(cklimit(sigmask) || ckoverflow(sigmask, sizeof(sigset_t)))
		return -EFAULT;

	current->blocked_saved = current->blocked;
	current->blocked = ((*sigmask) & ~CANNOTBLOCKSIG);

	current->sleep_spot = &wait;
	sleepon(&wait);

	//do this after we have done with the signal
	//current->blocked = set;

	return -EINTR;
}

/*
 * this function is special, notice it's return value, which will
 * be put in saved %eax in stack, if don't return regs.eax instead
 * of the return value of do_sigreturn, then page fault will happen.
 */
int sys_sigreturn(struct regs_t regs)
{
	do_sigreturn(&regs);

	return regs.eax;
}

int sys_kill(pid_t pid, int signum)
{
	if(signum < 1 || signum > SIGNR)
		return -EINVAL;

	if(!pid){
		if(current->euid != SU_UID)
			return -EPERM;
	}

	if(pid < 0){
		list_t * tmp, * pos;
		struct task_struct * t;

		if(pid >= -1)
			return -EINVAL;

		pid = -pid;

		if(!task[pid] || (task[pid]->pgid != pid))
			return -ESRCH;

		if((current->euid != SU_UID)
			&& (current->euid != task[pid]->euid)
			&& (current->uid != task[pid]->uid))
			return -EPERM;

		foreach(tmp, pos, &task[pid]->pglist)
			t = list_entry(tmp, pglist, struct task_struct);
			sendsig(t, signum);
		endeach(&task[pid]->pglist);

		return 0;	
	}

	if(!task[pid])
		return -ESRCH;

	if((current->euid != SU_UID)
		&& (current->euid != task[pid]->euid)
		&& (current->uid != task[pid]->uid))
		return -EPERM;

	sendsig(task[pid], signum);

	return 0;
}

pid_t sys_getpgrp(void)
{
	return current->pgid;
}

pid_t sys_getpgid(pid_t pid)
{
	struct task_struct * t;

	if(!pid)
		return current->pgid;

	if((pid < 0) || (pid > NR_TASKS - 1))
		return -EINVAL;

	t = task[pid];
	if(!t)
		return -ESRCH;

	if((current->pid != pid) && (current->pid != t->ppid))
		return -EPERM;

	return t->pgid;
}

extern struct tty_struct ttys[TTY_NUM];

int sys_setpgid(pid_t pid, pid_t pgid)
{
	int tty;
	struct task_struct * t = NULL;

	if((pid < 0) || (pid > NR_TASKS - 1))
		return -EINVAL;

	if((pgid < 0) || (pgid > NR_TASKS - 1))
		return -EINVAL;

	if(!task[pid] || !task[pgid])
		return -ESRCH;

	if(task[pid]->session != task[pgid]->session)
		return -EPERM;

	if(!pid)
		pid = current->pid;

	/* always in destination group, need to do nothing */
	if(task[pid]->pgid == pgid)
		return 0;

	if(pid == pgid){
		/* pid is the master, gpid has been used */
		if(task[pid]->pgid == pid) 
			return 0;
	}else if(task[pgid]->pid != task[pgid]->pgid) 
		/* process pgid must be master */
		return -EINVAL;

	t = task[pid];
	if((current->pid != pid) && (current->pid != t->ppid))
		return -EPERM;

	/* XXX: check child has call execve? */

	tty = t->tty;
	if(tty != NOTTY)
		tty -= 1;

	if(!list_empty(&t->pglist)){
		list_t * tmp, * pos;
		struct task_struct * t1;

		t1 = list_entry(t->pglist.next, pglist, struct task_struct);
		if((tty != NOTTY) && (t->pid == ttys[tty].t_owner))
			ttys[tty].t_owner = t1->pid;
	
		if(t->pid == t->pgid){
			pid_t pgid1 = t1->pid;

			list_foreach(tmp, pos, &t->pglist){
				t1 = list_entry(tmp,
					pglist, struct task_struct);
				t1->pgid = pgid1;
			}
		}

		// because process doesn't end its life, so use list_del1
		list_del1(&t->pglist);
	}else{
		if((tty != NOTTY) && (t->pid == ttys[tty].t_owner))
			ttys[tty].t_owner = 0;	
	}	

	t->pgid = pgid;
	t->tty = task[pgid]->tty;

	return 0;
}

pid_t sys_setsid(void)
{
	int ret;

	ret = sys_setpgid(current->pid, current->pid);
	if(ret < 0)
		return ret;

	current->tty = NOTTY;
	current->session = current->pid;

	return current->pid;
}

pid_t sys_getsid(pid_t pid)
{
	if((pid < 0) || (pid > NR_TASKS - 1))
		return -EINVAL;

	if(!pid)
		return current->session;

	if(!task[pid])
		return -ESRCH;

	if(current->session != task[pid]->session)
		return -EPERM;

	return task[pid]->session;
}

#if 0
pid_t sys_tcgetpgrp(int fd)
{
	printk("getpgrp\n");
	return 0;
}

int sys_tcsetpgrp(int fd, pid_t pgrpid)
{
	printk("setpgrp\n");
	return 0;
}
#endif

struct utsname{
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
};

int sys_uname(struct utsname * buf)
{
	if(cklimit(buf) || ckoverflow(buf, sizeof(struct utsname)))
		return -EFAULT;

	strcpy(buf->sysname, "cnix");
	strcpy(buf->nodename, "");
	strcpy(buf->release, "");
	strcpy(buf->version, "");
	strcpy(buf->machine, "i586");

	return 0;
}

int sys_brk(void * end_data_segment)
{
	return do_brk((unsigned long)end_data_segment);
}

pid_t sys_getpid(void)
{
	return current->pid;
}

pid_t sys_getppid(void)
{
	return current->ppid;
}

int sys_getgroups(int size, gid_t * list)
{
	if(cklimit(list) || ckoverflow(list, size * sizeof(gid_t)))
		return -EFAULT;

	if(size <= 0)
		return 0;

	// XXX: other places, need to add this check too
	if(!list)
		return -EFAULT;

	list[0] = current->gid;

	return 1;
}

int sys_setgroups(int size, const gid_t * list)
{
	if(cklimit(list) || ckoverflow(list, size * sizeof(gid_t)))
		return -EFAULT;

	if(size <= 0)
		return 0;

	if(!list)
		return -EFAULT;

	if((current->euid != SU_UID) && (current->egid != SU_GID))
		return -EPERM;

	current->gid = list[0];

	return 1;
}

uid_t sys_getuid(void)
{
	return current->uid;
}

int sys_setuid(uid_t uid)
{
	if(current->euid == SU_UID){
		current->uid = current->euid = current->suid = uid;
		return 0;
	}

	if((uid == current->uid) || (uid == current->suid)){
		current->euid = uid;
		return 0;
	}

	return -EPERM;
}

uid_t sys_geteuid(void)
{
	return current->euid;
}

uid_t sys_getgid(void)
{
	return current->gid;
}

int sys_setgid(gid_t gid)
{
	if(current->egid == SU_UID){
		current->gid = current->egid = current->sgid = gid;
		return 0;
	}

	if((gid == current->gid) || (gid == current->sgid)){
		current->egid = gid;
		return 0;
	}

	return -EPERM;
}

uid_t sys_getegid(void)
{
	return current->egid;
}

int sys_setreuid(uid_t ruid, uid_t euid)
{
	if(current->euid == SU_UID){
		current->uid = ruid;
		current->euid = euid;
		return 0;
	}

	if((ruid != current->uid) && (ruid != current->euid))
		return -EPERM;

	if((euid != current->uid) && (euid != current->suid))
		return -EPERM;

	current->uid = ruid;
	current->euid = euid;

	return 0;
}

int sys_setregid(gid_t rgid, gid_t egid)
{
	if(current->egid == SU_UID){
		current->gid = rgid;
		current->egid = egid;
		return 0;
	}

	if((rgid != current->gid) && (rgid != current->egid))
		return -EPERM;

	if((egid != current->gid) && (egid != current->sgid))
		return -EPERM;

	current->gid = rgid;
	current->egid = egid;

	return 0;
}

time_t sys_time(time_t * t)
{
	if(cklimit(t) || ckoverflow(t, sizeof(struct tms)))
		return -EFAULT;

	if(t){
		*t = curclock();
		return *t;
	}

	return curclock();
}

clock_t sys_times(struct tms * buff)
{
	unsigned long flags;

	if(cklimit(buff) || ckoverflow(buff, sizeof(struct tms)))
		return -EFAULT;

	lockb_timer(flags);

	buff->tms_utime = current->utime;
	buff->tms_stime = current->stime;

	buff->tms_cutime = current->cutime;
	buff->tms_cstime = current->cstime;

	unlockb_timer(flags);

	return nowticks;
}

int sys_settimeofday(const struct timeval * tv, const struct timezone * tz)
{
	if(cklimit(tv) || ckoverflow(tv, sizeof(struct timeval))
		|| cklimit(tz) || ckoverflow(tz, sizeof(struct timezone)))
		return -EFAULT;

	/* cmos write */

	return 0;
}

int sys_gettimeofday(struct timeval * tv, struct timezone * tz)
{
	if(cklimit(tv) || ckoverflow(tv, sizeof(struct timeval))
		|| cklimit(tz) || ckoverflow(tz, sizeof(struct timezone)))
		return -EFAULT;

	tv->tv_sec = curclock();
	tv->tv_usec = (nowticks % HZ) * (1000000 / HZ);

	/* don't care tz */

	return 0;
}

mode_t sys_umask(mode_t mask)
{
	if (mask == 0)
	{
		return current->umask;
	}
	
	current->umask = mask;
	return mask;
}

int sys_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout)
{
	if (cklimit(readset) || ckoverflow(readset, sizeof(fd_set)) || \
		cklimit(writeset) || ckoverflow(writeset, sizeof(fd_set)) || \
		cklimit(exceptset) || ckoverflow(exceptset, sizeof(fd_set)) || \
		cklimit(timeout) || ckoverflow(timeout, sizeof(struct timeval)))
	{
		return -EFAULT;
	}

	return do_select(maxfdp1, readset, writeset, exceptset, timeout);
}

int sys_poll(struct pollfd * fds, unsigned long nfds, int timeout)
{
	int i;

	/*
	 * XXX: strange, cklimit don't check if fds is NULL or not,
	 * but no matters.
	 */
	if(cklimit(fds))
		return -EFAULT;

	if(nfds == 0)
		return 0;

	for(i = 0; i < nfds; i++){
		if(ckfdlimit(fds[i].fd))
			return -EINVAL;

		if(!fget(fds[i].fd))
			return -EBADF;
	}

	return do_poll(fds, nfds, timeout);
}

int sys_getrlimit(int resource, struct rlimit * rlim)
{
	if((resource < 0) || (resource >= RLIM_NLIMITS))
		return -EINVAL;

	if(cklimit(rlim) || ckoverflow(rlim, sizeof(struct rlimit)))
		return -EFAULT;

	*rlim = current->rlims[resource];

	return 0;
}

int sys_setrlimit(int resource, const struct rlimit * rlim)
{
	struct rlimit * rl;

	if((resource < 0) || (resource >= RLIM_NLIMITS)) 
		return -EINVAL;

	if(cklimit(rlim))
		return -EFAULT;

	if(rlim->rlim_cur > rlim->rlim_max)
		return -EINVAL;

	rl = &current->rlims[resource];

	if((current->euid != SU_UID) && (rlim->rlim_max > rl->rlim_max))
		return -EPERM;

	rl->rlim_max = rlim->rlim_max;
	rl->rlim_cur = rlim->rlim_cur;

	return 0;
}

int sys_syslog(int priority, const char *buff, int len)
{
	return do_user_syslog(priority, buff, len);
}

// xxx: invalidate
static void * __sys_mmap(
	void * start, size_t length, int prot, int flags, int fd, off_t offset
	)
{
	int ret;

	if(!length)
		return (void *)-EINVAL;

	if(start){
		if(cklimit(start) || ckoverflow(start, length))
			return (void *)-EFAULT;

		if((unsigned long)start < current->mm.end_data)
			return (void *)-EINVAL;
	}

	ret = do_mmap(
		(unsigned long)start,
		length,
		prot,
		flags,
		fd,
		offset,
		length, // memsz
		current->pg_dir,
		&current->mm
		);

	return (void*)ret;
}

void * sys_mmap(unsigned long * args)
{
	return __sys_mmap(
		(void *)args[0],
		(size_t)args[1],
		(int)args[2],
		(int)args[3],
		(int)args[4],
		(off_t)args[5]
		);
}

int sys_munmap(void * start, size_t length)
{
	if(!start || (length <= 0))
		return -EINVAL;

	if(cklimit(start) || ckoverflow(start, length))
		return -EFAULT;

	return do_munmap((unsigned long)start, length);	
}

int sys_msync(void * start, size_t length, int flags)
{
	if(!start)
		return -EINVAL;

	if(cklimit(start) || ckoverflow(start, length))
		return -EFAULT;

	return do_msync((unsigned long)start, length, flags);
}

int sys_ioperm(struct regs_t regs)
{
	unsigned long from = regs.ebx;
	unsigned long num = regs.ecx;
	int turn_on = (int)regs.edx;
	unsigned char * bitmap;
	int m, n;
	static unsigned char bits[] = { 1, 3, 7, 15, 31, 63, 127, 255 };

	if((from > 0x3ff) || ((from + num) > 0x3ff))
		return -EINVAL;

	if(current->euid != SU_UID)
		return -EACCES;

	if(!num)
		return 0;

	bitmap = current->bitmap;

	m = from % 8;
	if(m > 0){
		n = 8 - m;
		if(num < n)
			n = num;

		if(turn_on)
			bitmap[from / 8] &= ~(bits[n] << m);
		else
			bitmap[from / 8] |= bits[n] << m;

		if(num <= n){
			// xxx: ltr again?
			memcpy(tss.bitmap, current->bitmap, 128);
			return 0;
		}

		from += n;
		num -= n;
	}

	memset(&bitmap[from / 8], turn_on ? 0x00 : 0xff, num / 8);

	from += (num / 8) * 8;
	n = num % 8;

	if(n > 0){
		if(turn_on)
			bitmap[from / 8] &= ~bits[n];
		else
			bitmap[from / 8] |= bits[n];
	}

	// xxx: ltr again?
	memcpy(tss.bitmap, current->bitmap, 128);

	return 0;
}

int sys_iopl(struct regs_t regs)
{
	int level = regs.ebx;

	if((level < 0) || (level > 3))
		return -EINVAL;

	if(current->euid != SU_UID)
		return -EACCES;

	regs.eflags &= ~(0x3 << 12);
	regs.eflags |= level << 12;

	return 0;
}

int sys_null_call(struct regs_t regs)
{
	printk("%s(%d) null syscall number %d, at eip %x...\n",
		current->myname, current->pid, regs.index, regs.eip);

	return -1;
}

void syscall_print(struct regs_t regs)
{
	__asm__("pushl %eax\n");

	if(strstr(current->myname, "vim"))
		printk(
			"%x: %s(%d) syscall number %d at eip %x...\n",
			nowticks,
			current->myname, current->pid,
			regs.index, regs.eip
		);

	__asm__("popl %eax\n");
}
