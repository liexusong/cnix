#ifndef _SIGNAL_H
#define _SIGNAL_H

typedef unsigned int sigset_t;

#define SIGNR	32

#define NSIG	16

#define SIGHUP		1
#define SIGINT		2
#define SIGQUIT		3
#define SIGILL		4
#define SIGTRAP		5
#define SIGABRT		6
#define SIGIOT		6
#define SIGBUS		7
#define SIGFPE		8
#define SIGKILL		9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGSTKFLT	16

#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22

#define SIGURG		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGIO		29
#define SIGPOLL		SIGIO
/*
#define SIGLOST		29
*/
#define SIGPWR		30
#define SIGSYS		31
#define	SIGUNUSED	31

#define SA_NOCLDSTOP	0x00000001
#define SA_NOCLDWAIT	0x00000002
#define SA_SIGINFO	0x00000004
#define SA_ONSTACK	0x08000000
#define SA_RESTART	0x10000000
#define SA_NODEFER	0x40000000
#define SA_RESETHAND	0x80000000

#define SIG_BLOCK	0
#define SIG_UNBLOCK	1
#define SIG_SETMASK	2

#define WNOHANG		1
#define WUNTRACED	2

union sigval{
	int	sival_int;
	void *	sival_ptr; 
};

typedef struct siginfo{
	int		si_signo;
	int		si_code;
	union sigval	si_value;
}siginfo_t;

typedef void (*sighandler_t)(int, siginfo_t *, void *);

#define SIG_DFL	((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

struct sigaction{
	void (*sa_handler)(int, siginfo_t *, void *);
	sigset_t sa_mask;
	int sa_flags;
};

struct sigframe{
	unsigned long sf_retaddr;
	siginfo_t sf_siginfo;
	char sf_retcode[32];
	unsigned long sf_unused; /* pretend to be a return addr for sigreturn */
};

#endif
