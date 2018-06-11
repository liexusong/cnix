#include <cnix/unistd.h>
#include <cnix/const.h>
#include <cnix/string.h>
#include <cnix/stdarg.h>
#include <asm/system.h>
#include <cnix/wait.h>
#include <cnix/driver.h>
#include <cnix/kernel.h>
#include <cnix/fs.h>
#include <cnix/errno.h>
#include <cnix/verno.h>
#include <cnix/net.h>

unsigned long start_mem,end_mem;

extern void traps_init(void);
extern void syscall_init(void);
extern void intr_init(void);
extern void tty_init(void);
extern void timer_init(void);
extern void ide_init(void);
extern void buf_init(void);
extern void fs_init(void);
extern void reboot(void);
extern void get_mem(unsigned long *,unsigned long *);
extern unsigned long paging_init(unsigned long ,unsigned long);
extern void mem_init(unsigned long , unsigned long);
extern void show_area();
extern unsigned long get_free_pages(int,int);
extern char *kmalloc(int size,int flags);
extern void kfree(char *p);
extern void idle_task(void);
extern void net_init(void);
extern void ip_reply_daemon(void *data);
extern void klogd(void *data);

int errno;	

_syscall3(int, open, const char *, filename, int, flags, mode_t, mode)
_syscall1(int, dup, int, fd)
_syscall3(int, mount, const char *, specfile, const char *, dirname, int, flag)
_syscall3(int, execve, char *, pathname, char **, argv, char **, envp)
_syscall3(int, waitpid, pid_t, pid, int *, status, int, option)
_syscall0(int, setsid)

static void gather_bios(void);
static void init(void *);

#define wait(x)	waitpid(-1, x, 0)

int main(void)		
{	
	/* traps_init and intr_init must be executed first. */
	traps_init();
	syscall_init();
	intr_init();
	
	/* it's ok to enable interrupt. */
	sti();	

	tty_init();
	
	printk("%s", version); 

	/* gather the information gotten by BIOS before mem_init */
	gather_bios();

	get_mem(&start_mem, &end_mem);
	printk("The system's memory is %d M\n", (end_mem >> 20));
	start_mem = paging_init(start_mem, end_mem);
	mem_init(start_mem, end_mem);
#if 0
	show_area();
#endif

	buf_init();

	init_bottom();

	timer_init();

	/* after timer_init */
	ide_init();

	fs_init();

	sched_init();

	net_init();

	kernel_thread(init, NULL);

	/* idle */
	idle_task();		// never return;

	return 0;
}

#define INIT_FUNC_NUM 256

extern void get_ide_bios_info(void);

struct init_func{
	char * desc;
	void (*func)(void);
}init_funcs[INIT_FUNC_NUM] = {
	{"get_ide_bios_info", get_ide_bios_info},
	{NULL, NULL},
};

static void gather_bios(void)
{
	int i;
	
	for(i = 0; init_funcs[i].func; i++)
		init_funcs[i].func();
}

static void kernel_execve(char * filename, char * argv[], char * envp[])
{
	current->kernel = 0;
	execve(filename, argv, envp);
	current->kernel = 1;
}

static void shell(void * data)
{
	int tty, fd;
	char ttyname[32];
	char * argv[2] = { "/bin/login", NULL };

	tty = (int)data;

	setsid();

	sprintf(ttyname, "/dev/tty%d", tty);

	nochk();

	if((fd = open(ttyname, O_RDWR, 0)) < 0)
		DIE("open %s error\n", ttyname);	

	do_dup(fd);
	do_dup(fd);

	kernel_execve("/bin/login", argv, (char **)NULL);

	rechk();
		
	panic("not found /bin/login\n");
}

static void bflushd_timeout(void * data)
{
	struct wait_queue ** wait;

	wait = data;
	wakeup(wait);
}

#define BFLUSH_TIME_INTERVAL 10

static void bflushd(void * data)
{
	struct wait_queue * wait = NULL;
	synctimer_t sync;

	strcpy(current->myname, "bflushd");

	while(1){
		sync.expire = BFLUSH_TIME_INTERVAL * HZ;
		sync.data = &wait;
		sync.timerproc = bflushd_timeout;
		synctimer_set(&sync);

		sleepon(&wait);
		bsync();
#if 0
		{
			extern void invoke_arp_req(void);
			invoke_arp_req();
		}
#endif
	}
}

/*
 * login should be a user-state process
 */
static void login(void * data)
{
	int status, pid, pid0, pid1, pid2;	

	strcpy(current->myname, "login");

	pid0 = kernel_thread(shell, (int *)0);
	pid1 = kernel_thread(shell, (int *)1);

	do{
		nochk();
		pid = wait(&status);
		rechk();

		if(pid < 0){
			schedule();
			continue;
		}
forkagain:
		pid2 = kernel_thread(shell, (int *)((pid == pid0) ? 0 : 1));
		if(pid2 > 0){
			if(pid == pid0)
				pid0 = pid2;
			else
				pid1 = pid2;
		}else{
			/* sleep a while, and schedule */
			schedule();
			goto forkagain;
		}
	}while(1);

}

static void init(void * data)
{
	int status;	
	struct inode * inoptr;

	strcpy(current->myname, "init");

	inoptr = iget(ROOT_DEV, ROOT_INODE);
	current->root = current->pwd = inoptr;
	inoptr->i_count += 2;	/* root pwd take 2 */
	iput(inoptr);

	printk("mount /dev/hd0 as root directory\n");

	nochk();
	mount("/dev/hd0", "/", O_RDWR);
	rechk();

	printk("\n\t\t\t\t\033[35mWelcome to CNIX!\033[0m\n\n");

	kernel_thread(bflushd, NULL);
	kernel_thread(login, NULL);
	kernel_thread(ip_reply_daemon, NULL);
	kernel_thread(klogd, NULL);

#if TCP_TEST
	{
		extern void tcp_server(void * data);
		extern void tcp_client(void * data);

		kernel_thread(tcp_server, NULL);
		kernel_thread(tcp_client, NULL);
	}
#endif

	while(1){
		nochk();
		if(wait(&status) == -ECHILD)
			schedule();
		rechk();
	}
}
