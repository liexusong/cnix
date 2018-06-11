#ifndef _UNISTD_H
#define _UNISTD_H

#ifndef NULL
#define NULL    ((void *)0)
#endif

#define __NR_exit		1
#define __NR_fork		2
#define __NR_read		3
#define __NR_write		4
#define __NR_open		5
#define __NR_close		6
#define __NR_waitpid		7
#define __NR_creat		8
#define __NR_link		9
#define __NR_unlink		10
#define __NR_execve		11
#define __NR_chdir		12
#define __NR_time		13
#define __NR_mknod		14
#define __NR_chmod		15
#define __NR_chown		16
#define __NR_break		17
#define __NR_oldstat		18
#define __NR_lseek		19
#define __NR_getpid		20
#define __NR_mount		21
#define __NR_umount		22
#define __NR_setuid		23
#define __NR_getuid		24
#define __NR_stime		25
#define __NR_ptrace		26
#define __NR_alarm		27
#define __NR_oldfstat		28
#define __NR_pause		29
#define __NR_utime		30
#define __NR_stty		31
#define __NR_gtty		32
#define __NR_access		33
#define __NR_nice		34
#define __NR_ftime		35
#define __NR_sync		36
#define __NR_kill		37
#define __NR_rename		38
#define __NR_mkdir		39
#define __NR_rmdir		40
#define __NR_dup		41
#define __NR_pipe		42
#define __NR_times		43
#define __NR_prof		44
#define __NR_brk		45
#define __NR_setgid		46
#define __NR_getgid		47
#define __NR_signal		48
#define __NR_geteuid		49
#define __NR_getegid		50
#define __NR_acct		51
#define __NR_phys		52
#define __NR_lock		53
#define __NR_ioctl		54
#define __NR_fcntl		55
#define __NR_mpx		56
#define __NR_setpgid		57
#define __NR_ulimit		58
#define __NR_uname		59
#define __NR_umask		60
#define __NR_chroot		61
#define __NR_ustat		62
#define __NR_dup2		63
#define __NR_getppid		64
#define __NR_getpgrp		65
#define __NR_setsid		66
#define __NR_sigaction		67
#define __NR_sgetmask		68
#define __NR_ssetmask		69
#define __NR_setreuid		70
#define __NR_setregid		71
#define __NR_sigsuspend		72
#define __NR_sigpending		73
#define __NR_sethostname	74
#define __NR_setrlimit		75
#define __NR_getrlimit		76
#define __NR_getrusage		77
#define __NR_gettimeofday	78
#define __NR_settimeofday	79
#define __NR_getgroups		80
#define __NR_setgroups		81
#define __NR_oldselect		82
#define __NR_symlink		83
#define __NR_oldlstat		84
#define __NR_readlink		85

#define __NR_mmap		90
#define __NR_munmap		91

#define __NR_fchmod		94
#define __NR_fchown		95

#define __NR_statfs		99
#define __NR_fstatfs		100
#define __NR_ioperm		101
#define __NR_socketcall		102
#define __NR_syslog		103

#define __NR_stat		106
#define __NR_lstat		107
#define __NR_fstat		108

#define __NR_iopl		110

#define __NR_fsync		118
#define __NR_sigreturn		119

#define __NR_sigprocmask	126

#define __NR_getpgid		132
#define __NR_fchdir		133

#define __NR_getdents		141
#define __NR_select		142
#define __NR_readv		145
#define __NR_writev		146
#define __NR_getsid		147

#define __NR_poll		168

#define __NR_getcwd		183

#define NR_SYSCALLS	256

#define _syscall0(type,name) \
type name(void) \
{ \
type __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name)); \
if (__res >= 0) \
	return __res; \
errno = -__res; \
return -1; \
}

#define _syscall1(type,name,atype,a) \
type name(atype a) \
{ \
type __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" (a)); \
if (__res >= 0) \
	return __res; \
errno = -__res; \
return -1; \
}

#define _syscall2(type,name,atype,a,btype,b) \
type name(atype a,btype b) \
{ \
type __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" (a),"c" (b)); \
if (__res >= 0) \
	return __res; \
errno = -__res; \
return -1; \
}

#define _syscall3(type,name,atype,a,btype,b,ctype,c) \
type name(atype a,btype b,ctype c) \
{ \
type __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" (a),"c" (b),"d" (c)); \
if (__res<0) \
	errno=-__res , __res = -1; \
return __res;\
}

#define _syscall4(type,name,atype,a,btype,b,ctype,c, dtype, d) \
type name(atype a,btype b,ctype c, dtype d) \
{ \
type __res; \
__asm__ volatile ("pushl %esi\n\t");\
__asm__ volatile ( "int $0x80\n\t" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" (a),"c" (b),"d" (c),"S"(d)); \
__asm__ volatile ("popl %esi\n\t");\
if (__res<0) \
	errno=-__res , __res = -1; \
return __res;\
}

#define _syscall5(type,name,atype,a,btype,b,ctype,c, dtype, d, etype, e) \
type name(atype a,btype b,ctype c, dtype d, etype e) \
{ \
type __res; \
__asm__ volatile ("pushl %esi\n\t");\
__asm__ volatile ("pushl %edi\n\t");\
__asm__ volatile ( "int $0x80\n\t" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" (a),"c" (b),"d" (c), "S"(d), "D"(e)); \
__asm__ volatile ("popl %edi\n\t");\
__asm__ volatile ("popl %esi\n\t");\
if (__res<0) \
	errno=-__res , __res = -1; \
return __res;\
}

extern int errno;

#endif
