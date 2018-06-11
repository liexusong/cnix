#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	pid_t pid1, pid2;
	char *args[3];
	char *child_args[3];
	int status;

	args[0] = "shell";
	args[1] = NULL;

	child_args[0] = "ls";
	child_args[1] = "-l";
	child_args[2] = NULL;
	
	printf("before fork()\n");

	if ((pid1 = fork()) != 0)
	{
		pid2 = fork();
		if (pid2 != 0)
		{
			for (;;)
			{
				if (waitpid(pid1, &status, 0) < 0)
				{
					printf("errno = %d\n", errno);
					
					if (errno == EINTR)
					{
						continue;
					}
					else
					{
						break;
					}
				}
				else
				{
					if (WIFEXITED(status))
					{
						printf("exit status: %d\n", WEXITSTATUS(status));
					}
				}
			}
			/* pid2 is zombie */
		}
		else
		{
			if (execve("/bin/ls", child_args, NULL) < 0)
			{
				printf("execve error!\n");
			}
		}
	}
	else
	{
		if (execve("/bin/shell", args, NULL) < 0)
		{
			printf("execve error!\n");
		}
		
	}

	return 0;
}
