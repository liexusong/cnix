#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char * argv[])
{
	struct sockaddr_in s_addr;
	int sock;
	int addr_len;
	int len;
	char buff[15];

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stdout, "socket error %s\n", strerror(errno));
		exit(-1);
	}

	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(8888);
	if(argc >= 2)
		s_addr.sin_addr.s_addr = inet_addr(argv[1]);
	else
		s_addr.sin_addr.s_addr = inet_addr("192.168.23.123");

	addr_len = sizeof(s_addr);

	if(connect(sock, (struct sockaddr *)&s_addr, addr_len) < 0){
		close(sock);
		fprintf(stderr, "connect error %s\n", strerror(errno));
		return -1;
	}

	strcpy(buff, "helloworldabcd");

	len = write(sock, buff, strlen(buff));
	if (len < 0)
		fprintf(stdout, "write error %s\n", strerror(errno));
	else
		printf("send succes\n");

	close(sock);

	return 0;
} 
