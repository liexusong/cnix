#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

void sigchld(int signum)
{
	printf("get SIGCHLD\n");
}

int main(void)
{
	signal(SIGCHLD, sigchld);

	if(!fork()){
		alarm(2);
		pause();
		exit(0);
	}

	pause();

	return 0;
}
