/* This tool is used to make a.img */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define FD0_SIZE 1474560 /* 1.44M */
#define BUFSIZE 512

#define DEBUG 0

int main(int argc, char **argv)
{
	int fd_in, fd_out;
	int n;
	int size = 0;
	char buf[BUFSIZE];

	if (argc != 4) {
		printf("Usage:\t%s boot system outfile\n",
		       argv[0]);
		exit(-1);
	}

	fd_in = open(argv[1], O_RDONLY);
	if (fd_in == -1) {
		printf("Can't open %s\n", argv[1]);
		exit(-1);
	}

	fd_out = open(argv[3], O_WRONLY | O_CREAT, 0600);
	if (fd_out == -1) {
		printf("Can't open %s\n", argv[3]);
		close(fd_in);
		exit(-1);
	}

	n = read(fd_in, buf, BUFSIZE);
	if(n != 512){
		printf("Read %s error!", argv[1]);
		close(fd_in);
		close(fd_out);
		exit(-1);
	}
	
	if(write(fd_out, buf, n) != n){
		printf("Write %s error.\n", argv[3]);
		close(fd_in);
		close(fd_out);
		exit(-1);
	}
	
	size += BUFSIZE;

	close(fd_in);

	fd_in = open(argv[2], O_RDONLY);
	if (fd_in == -1) {
		printf("Can't open %s\n", argv[2]);
		close(fd_out);
		exit(-1);
	}

#if DEBUG
	printf("Begin to read %s...\n", argv[2]);
#endif

	while (1) {
		n = read(fd_in, buf, BUFSIZE);
		if (n == 0 || n == -1)
			break;
		size += n;

		if (write(fd_out, buf, n) != n) {
			printf("write error.\n");
			close(fd_in);
			close(fd_out);
			exit(-1);
		}
	}

#if DEBUG
	printf("read %s done\n", argv[2]);
#endif
	
	if (size > FD0_SIZE) {
		printf("em..%s is bigger than 1.4M.\n", argv[3]);
		close(fd_in);
		close(fd_out);
		exit(-1);
	}

#if DEBUG
	printf("Begin to lseek...\n");
#endif

	n = FD0_SIZE - size - 2;
	n = lseek(fd_out, n, SEEK_CUR);
	if (n == -1)
		printf("Lseek error\n");

#if DEBUG
	printf("lseek done: %d\n", n);
#endif

	write(fd_out, "dd", 2);
	close(fd_in);
	close(fd_out);
	return 0;
}
		
