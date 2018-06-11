#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include "setjmp.h"

sigjmp_buf g_jmp_buff;

void test_jmp(void)
{
	int x[1000];

	memset(x, 0, sizeof(x));

	siglongjmp(g_jmp_buff, 2);

}


void alarm_handler(int x)
{

	printf("alarm begin!\n");

	alarm(1);

	test_jmp();	
}

int main(void)
{
	volatile int i;

	i = 0;


	if (sigsetjmp(g_jmp_buff, 10))
	{
		printf("ret from int jmp!\n");
	}

{
	sigset_t mask;

	sigemptyset(&mask);

	if (sigprocmask(0, NULL, &mask) < 0)
	{
		printf("sigprocmask error");
	}

	if (sigismember((sigset_t *)&mask, SIGALRM))
	{
		printf("SIGALRM is set!\n");
	}

	sigdelset(&mask, SIGALRM);

	if (sigprocmask(SIG_SETMASK, &mask, NULL) < 0)
	{
		printf("sigprocmask error!\n");
	}

	sigemptyset(&mask);
	if (sigprocmask(0, NULL, &mask) < 0)
	{
		printf("sigprocmask error");
	}

	if (sigismember((sigset_t *)&mask, SIGALRM))
	{
		printf("SIGALRM is set!\n");
	}
}
	
	printf("here!\n");

	if (signal(SIGALRM, alarm_handler) == SIG_ERR)
	{
		printf("signal error!\n");
		return 1;
	}
	alarm(1);
	
	for (;;)
	{
		pause();
	}

	return 1;
}
