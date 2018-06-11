#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/sched.h>
#include <cnix/signal.h>
#include <cnix/kernel.h>

#define TO_IGN	0
#define TO_KILL	1
#define TO_CORE	2

static int def_action[32] = {
	TO_KILL,	// SIGHUP
	TO_KILL,	// SIGINT
	TO_KILL,	// SIGQUIT
	TO_KILL,	// SIGILL
	TO_KILL,	// SIGTRAP
	TO_KILL,	// SIGABRT
	TO_KILL,	// SIGUNUSED
	TO_KILL,	// SIGFPE
	TO_KILL,	// SIGKILL
	TO_KILL,	// SIGUSR1
	TO_KILL,	// SIGSEGV
	TO_KILL,	// SIGUSR2
	TO_KILL,	// SIGPIPE
	TO_KILL,	// SIGALRM
	TO_KILL,	// SIGTERM
	TO_KILL,	// SIGSTKFLT
	TO_IGN,		// SIGCHLD
	TO_IGN,		// SIGCONT xx
	TO_IGN,		// SIGSTOP xx
	TO_IGN,		// SIGTSTP xx
	TO_IGN,		// SIGTTIN xx
	TO_IGN,		// SIGTTOU xx
	TO_KILL,
	TO_KILL,
	TO_KILL,
	TO_KILL,
	TO_KILL,
	TO_KILL,
	TO_KILL,
	TO_KILL,
	TO_KILL,
	TO_KILL,
};

/* movl $__NR_sigreturn, %eax; int $0x80 */
static char retcode[] = { 0xb8, 0x77, 0x00, 0x00, 0x00, 0xcd, 0x80 };

extern void restore_all(void);
extern int do_exit(int);

void do_with_signal(struct regs_t oldregs)
{
	int signum;
	unsigned long flags;
	sighandler_t handler;

	long esp;
	struct sigframe * frameptr;
	volatile struct regs_t * oldrptr; /* don't optimise */
	struct regs_t * regsave;

	lockb_all(flags);

	if(!current->signal)
		DIE("can't happen\n");

	signum = -1;

check_again:
	for(signum += 1; signum < SIGNR; signum++){
		if((current->signal & (1 << signum))
			&& !(current->blocked & (1 << signum)))
			break;
	}

	if(signum >= SIGNR){
		unlockb_all(flags);
		return;
	}

	current->signal &= ~(1 << signum);

	if(current->pid <= 1){	
		unlockb_all(flags);

		printk("process[%d] get a signal %d\n",current->pid, signum);
		return;
	}

	handler = current->sigaction[signum].sa_handler;
	if(handler == SIG_IGN){
		if(current->signal)
			goto check_again;

		unlockb_all(flags);
		return;
	}

	if(handler == SIG_DFL){
		switch(def_action[signum]){
		case TO_IGN:
			if(current->signal)
				goto check_again;

			unlockb_all(flags);
			return;
		case TO_KILL:
		case TO_CORE:
			unlockb_all(flags);
			do_exit(signum + 1);
			return;
		default:
			DIE("BUG: cannot happen\n");
			break;
		}
	}

	unlockb_all(flags);

#if 0
	current->sigaction[signum].sa_handler = SIG_DFL;
#endif

	current->blocked_saved = current->blocked;
	current->blocked = current->sigaction[signum].sa_mask;

	/* XXX: if user stack is not enough, how to do? */ 

	esp = oldregs.esp;

	/* check addr limit */

	regsave = (struct regs_t *)esp - 1;

	*regsave = oldregs;

	frameptr = (struct sigframe *)regsave - 1;

	frameptr->sf_retaddr = (unsigned long)frameptr->sf_retcode;

	frameptr->sf_siginfo.si_signo = signum + 1;
	frameptr->sf_siginfo.si_code = -1; // XXX
	frameptr->sf_siginfo.si_value.sival_int = 0; // XXX

	memcpy(&frameptr->sf_retcode[0], retcode, sizeof(retcode));

	frameptr->sf_unused = 0;

	oldrptr = &oldregs;

	oldrptr->eip = (long)handler;
	oldrptr->esp = (long)frameptr;
}

int do_sigreturn(struct regs_t * regs)
{
	struct sigframe * frameptr;
	struct regs_t * regsave;

	/* XXX: add more code to check regsave's credit */

	frameptr = (struct sigframe *)(regs->esp - 4) + 1; /* 4 for ret addr */
	regsave = (struct regs_t *)frameptr;

	if(regsave->cs == 0x1b){
		if(current->blocked_saved){
			current->blocked = current->blocked_saved;
			current->blocked_saved = 0;
		}

		*regs = *regsave;
		regs->ds = 0x23;
		regs->es = 0x23;
		regs->ss = 0x23;
	}else
		printk("error in do_sigreturn\n");

	return 0;
}

BOOLEAN anysig(struct task_struct * task)
{
	int signum;
	unsigned long flags;
	sighandler_t handler;

	lockb_all(flags);

	if(!current->signal){
		unlockb_all(flags);
		return FALSE;
	}

	signum = -1;

check_again:
	for(signum += 1; signum < SIGNR; signum++){
		if((current->signal & (1 << signum))
			&& !(current->blocked & (1 << signum)))
			break;
	}

	//current->signal &= ~(1 << signum);

	if(signum >= SIGNR){
		unlockb_all(flags);
		return FALSE;
	}

	handler = current->sigaction[signum].sa_handler;
	if(handler == SIG_IGN){
		if(current->signal)
			goto check_again;

		unlockb_all(flags);
		return FALSE;
	}

	if(handler == SIG_DFL){
		switch(def_action[signum]){
		case TO_IGN:
			if(current->signal)
				goto check_again;

			unlockb_all(flags);
			return FALSE;
		case TO_KILL:
		case TO_CORE:
			unlockb_all(flags);
			return TRUE;
		default:
			DIE("BUG: cannot happen\n");
			break;
		}
	}

	unlockb_all(flags);

	return TRUE;
}

void sendsig(struct task_struct * task, int signum)
{
	unsigned long flags;
	sighandler_t handler;

	if(signum <= 0)
		DIE("BUG: cannot happen\n");

	signum -= 1;

	lockb_all(flags);

	handler = task->sigaction[signum].sa_handler;
	if((handler == SIG_IGN)
		|| ((handler == SIG_DFL) && (def_action[signum] == TO_IGN))){
		unlockb_all(flags);
		return;
	}

	task->signal |= 1 << signum;

	unlockb_all(flags);

	if(task != current)
		wakeup(task->sleep_spot);
}
