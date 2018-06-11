#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

sigjmp_buf g_jmp_buff;

void test_jmp(void)
{
	int x[1000];

	memset(x, 0, sizeof(x));

	siglongjmp(g_jmp_buff, 2);
}

int t1(void)
{
	int x[500];

	memset(x, 0, sizeof(x));

	if (sigsetjmp(g_jmp_buff, 1))
	{
		printf("ret from int jmp!\n");
	}

	return 0;
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


	if (signal(SIGALRM, alarm_handler) == SIG_ERR)
	{
		printf("signal error!\n");
		return 1;
	}
	
	t1();
	printf("here!\n");

	alarm(1);
	
	for (;;)
	{
		pause();
	}

	return 1;
}
