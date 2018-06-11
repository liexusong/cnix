#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char * argv[])
{
	struct sockaddr_in s_addr;
	int sock;
	int addr_len;
	int len;
	char namelen;
	char buffer[1025];
	int fd;

	if(argc < 3){
		printf("get ip filename [port]\n");
		return -1;
	}

	namelen = (char)strlen(argv[2]);
	if(namelen < 0){
		printf("namelen is too long\n");
		return -1;
	}

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stdout, "socket error %s\n", strerror(errno));
		exit(-1);
	}

	s_addr.sin_family = AF_INET;

	if(argc >= 4)
		s_addr.sin_port = htons(atoi(argv[3]));
	else
		s_addr.sin_port = htons(8888);

	if(argc >= 2)
		s_addr.sin_addr.s_addr = inet_addr(argv[1]);

	addr_len = sizeof(s_addr);

	if ((fd = open(argv[2], O_WRONLY | O_CREAT, 0666)) < 0)
	{
		printf("open error %s\n", strerror(errno));
		close(sock);
		return 1;
	}

	if(connect(sock, (struct sockaddr *)&s_addr, addr_len) < 0){
		close(sock);
		fprintf(stderr, "connect error %s\n", strerror(errno));
		return -1;
	}

	write(sock, &namelen, 1);
	write(sock, argv[2], namelen);

	while((len = read(sock, buffer, 1024)) > 0){
		if (write(fd, buffer, len) != len)
		{
			printf("write error %s\n", strerror(errno));
		}

	}
	
	close(fd);

	close(sock);

	return 0;
} 
