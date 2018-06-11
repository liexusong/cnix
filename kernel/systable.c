#include <cnix/unistd.h>

unsigned long * syscall_table[NR_SYSCALLS];

#define FILL_SYSCALL(name) \
do{\
	extern unsigned long sys_##name; \
	(syscall_table[__NR_##name] = (unsigned long *)&sys_##name); \
}while(0)

extern void sys_null_call(void);

void syscall_init(void)
{
	int i;
	
	for(i = 0; i < NR_SYSCALLS; i++)
		syscall_table[i] = (unsigned long *)sys_null_call;

	FILL_SYSCALL(exit);
	FILL_SYSCALL(fork);
	FILL_SYSCALL(execve);
	FILL_SYSCALL(waitpid);

	FILL_SYSCALL(mount);
	FILL_SYSCALL(umount);

	FILL_SYSCALL(creat);
	FILL_SYSCALL(read);
	FILL_SYSCALL(write);
	FILL_SYSCALL(open);
	FILL_SYSCALL(lseek);
	FILL_SYSCALL(close);

	FILL_SYSCALL(dup);
	FILL_SYSCALL(dup2);
	FILL_SYSCALL(pipe);

	FILL_SYSCALL(fcntl);
	FILL_SYSCALL(ioctl);

	FILL_SYSCALL(access);
	FILL_SYSCALL(getcwd);
	FILL_SYSCALL(fsync);

	FILL_SYSCALL(link);
	FILL_SYSCALL(unlink);
	FILL_SYSCALL(rename);
	FILL_SYSCALL(symlink);
	FILL_SYSCALL(readlink);

	FILL_SYSCALL(chown);
	FILL_SYSCALL(fchown);
	FILL_SYSCALL(chmod);
	FILL_SYSCALL(fchmod);

	FILL_SYSCALL(mkdir);
	FILL_SYSCALL(rmdir);

	FILL_SYSCALL(chdir);
	FILL_SYSCALL(fchdir);
	FILL_SYSCALL(chroot);

	FILL_SYSCALL(mknod);
	FILL_SYSCALL(stat);
	FILL_SYSCALL(lstat);
	FILL_SYSCALL(fstat);

	FILL_SYSCALL(umask);

	FILL_SYSCALL(signal);
	FILL_SYSCALL(sigaction);
	FILL_SYSCALL(sigpending);
	FILL_SYSCALL(sigsuspend);
	FILL_SYSCALL(sigprocmask);
	FILL_SYSCALL(sigreturn);
	FILL_SYSCALL(kill);

	FILL_SYSCALL(alarm);
	FILL_SYSCALL(pause);

	FILL_SYSCALL(sync);

	FILL_SYSCALL(uname);

	FILL_SYSCALL(brk);

	FILL_SYSCALL(time);
	FILL_SYSCALL(times);
	FILL_SYSCALL(settimeofday);
	FILL_SYSCALL(gettimeofday);

	FILL_SYSCALL(getpid);
	FILL_SYSCALL(getppid);

	FILL_SYSCALL(setgroups);
	FILL_SYSCALL(getgroups);

	FILL_SYSCALL(getuid);
	FILL_SYSCALL(setuid);
	FILL_SYSCALL(geteuid);

	FILL_SYSCALL(getgid);
	FILL_SYSCALL(setgid);
	FILL_SYSCALL(getegid);

	FILL_SYSCALL(setreuid);
	FILL_SYSCALL(setregid);

	FILL_SYSCALL(getpgrp);
	FILL_SYSCALL(getpgid);
	FILL_SYSCALL(setpgid);
	FILL_SYSCALL(utime);

	FILL_SYSCALL(getdents);

	FILL_SYSCALL(select);
	FILL_SYSCALL(poll);

	FILL_SYSCALL(setsid);
	FILL_SYSCALL(getsid);

	FILL_SYSCALL(getrlimit);
	FILL_SYSCALL(setrlimit);

	FILL_SYSCALL(statfs);
	FILL_SYSCALL(fstatfs);

	FILL_SYSCALL(socketcall);
	FILL_SYSCALL(syslog);

	FILL_SYSCALL(mmap);
	FILL_SYSCALL(munmap);

	FILL_SYSCALL(ioperm);
	FILL_SYSCALL(iopl);
}
