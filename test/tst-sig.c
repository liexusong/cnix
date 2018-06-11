#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void sig_catcher(int sig);

void set_sighandler()
{
	struct sigaction act;
	struct sigaction oact;

	act.sa_handler = sig_catcher;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigemptyset(&oact.sa_mask);

	if (sigaction(SIGINT, &act, &oact) < 0)
	{
		printf("sigaction error!\n");
	}

}

void sig_catcher(int sig)
{
	printf("catcher signal!\n");

	set_sighandler();
}

int main(void)
{
	struct sigaction act;
	struct sigaction oact;

	act.sa_handler = sig_catcher;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigemptyset(&oact.sa_mask);

	if (sigaction(SIGINT, &act, &oact) < 0)
	{
		printf("sigaction error!\n");
	}

	
	for (;;)
	{
		pause();
	}

	return 0;
}
