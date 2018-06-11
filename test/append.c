#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char * argv[])
{
	int fd;

	if(argc != 3){
		printf("%s file\n", argv[0]);
		_exit(0);
	}

	fd = open(argv[1], O_RDWR);
	if(fd < 0){
		printf("%s not exist\n", argv[1]);
		_exit(0);
	}
	
	lseek(fd, 0, SEEK_END);
	write(fd, argv[2], strlen(argv[2]));
	write(fd, "\n", 1);
	
	close(fd);

	return 0;
}
