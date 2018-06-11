#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(void)
{
	int fd;

	fd = dup(1);
	write(fd, "hello", 6);
	getchar();
	++fd;
	dup2(1, fd);
	write(fd, "world\n", 6);

	return 0;
}
