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
	char buff[128];

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stdout, "socket error %s\n", strerror(errno));
		exit(-1);
	}

	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(8888);
	if(argc >= 2)
		s_addr.sin_addr.s_addr = inet_addr(argv[1]);
	else
		s_addr.sin_addr.s_addr = inet_addr("192.168.23.199");

	addr_len = sizeof(s_addr);
	strcpy(buff, "helloworldabcd");
	len = sendto(sock, buff, strlen(buff), 0,
		(struct sockaddr *) &s_addr, addr_len);
	if (len < 0)
		fprintf(stdout, "send error %s\n", strerror(errno));
	else
		printf("send succes\n");

	close(sock);

	return 0;
} 
