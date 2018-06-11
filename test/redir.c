#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(void)
{
	int fd;

	fd = open("hello.txt", O_CREAT | O_WRONLY);
	dup2(fd, 1);
	close(fd);
	printf("Hello World\n");
	fflush(stdout);

	return 0;
}
