#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int daemon_init(void)
{
	pid_t pid;

	if((pid = fork()) < 0)
		return -1;
	else if(pid != 0)
		exit(0);

	setsid();

	chdir("/");

	//umask(0);

	return 0;
}

int main(void)
{
	if(!daemon_init())
		for(;;);

	printf("daemon_init failed\n");

	return 0;
}
