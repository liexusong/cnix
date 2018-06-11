#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char * argv[])
{
	int fd, readbytes, i;
	char buffer[1024];

	if(argc != 2){
		printf("%s file\n", argv[0]);
		_exit(0);
	}
	
	fd = open(argv[1], O_RDONLY);
	if(fd < 0){
		printf("open file error!\n");
		_exit(0);
	}

	while((readbytes = read(fd, buffer, 1024)) > 0){

		for(i = 0; i < readbytes; i++)
			printf("%c", buffer[i]);
	}
	close(fd);

	return 0;
}
