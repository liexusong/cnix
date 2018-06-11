#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

void transfile(int fd)
{
	char buffer[1024];
	int file, namelen, len;

	if((len = read(fd, buffer, 1)) != 1){
		printf("get file name length error\n");
		return;
	}

	namelen = buffer[0];
	if(read(fd, buffer, namelen) < 0){
		printf("get file name error\n");
		return;
	}

	buffer[namelen] = '\0';
	printf("get file %s\n", buffer);

	file = open(buffer, O_RDONLY);
	if(file < 0){
		printf("open file error\n");
		return;
	}

	while((len = read(file, buffer, 1024)) > 0)
		if(write(fd, buffer, len) <= 0){
			printf("write error\n");
			break;
		}

	close(file);
}

int main(int argc, char * argv[])
{
	struct sockaddr_in s_addr;
	struct sockaddr_in c_addr;
	int sock, fd;
	int len, addr_len;
	char buff[101];

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stdout, "socket error %s\n", strerror(errno));
		return -1;
	} else
		printf("create socket\n");

	memset(&s_addr, 0, sizeof(struct sockaddr_in));

	s_addr.sin_family = AF_INET;
	if(argc > 1)
		s_addr.sin_port = htons(atoi(argv[1]));
	else
		s_addr.sin_port = htons(8888);
	s_addr.sin_addr.s_addr = INADDR_ANY;

	if ((bind(sock, (struct sockaddr *) &s_addr, sizeof(s_addr))) < 0) {
		fprintf(stdout, "bind error %s\n", strerror(errno));
		return -1;
	}

	listen(sock, 5);

	while ((fd = accept(sock, (struct sockaddr *)&c_addr, &addr_len)) > 0){
		transfile(fd);
		close(fd);
	}

	close(sock);

	return 0;
}
