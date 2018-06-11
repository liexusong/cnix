#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char * argv[])
{
	int fd;

	if(argc != 2){
		printf("%s file\n", argv[0]);
		_exit(0);
	}

	fd = open(argv[1], O_RDONLY, 0777);
	if(fd < 0){
		fd = open(argv[1], O_CREAT | O_WRONLY);
		if(fd < 0){
			printf("Please try again later\n");

			_exit(0);
		}
	}

	close(fd);

	return 0;
}
