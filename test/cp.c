#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char ** argv)
{
	int fd_in, fd_out, readbytes, readed;
	char buffer[1024];

	if(argc < 3){
		printf("%s srcfile dstfile\n", argv[0]);
		_exit(0);
	}

	fd_in = open(argv[1], O_RDONLY);

	if (fd_in < 0)
	{
		printf("open %s failed.\n", argv[1]);
		return 1;
	}
	fd_out = open(argv[2], O_CREAT | O_WRONLY);
	if (fd_out < 0)
	{
		printf("open %s failed.\n", argv[2]);
		close(fd_in);
		return 1;
	}

	readed = 0;
	while((readbytes = read(fd_in, buffer, 1024)) > 0){
		readed += readbytes;
		write(fd_out, buffer, readbytes);
	}
	
	close(fd_in);
	close(fd_out);

	return 0;
}
