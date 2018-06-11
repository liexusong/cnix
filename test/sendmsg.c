#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>

int main(int argc, char * argv[])
{
	struct sockaddr_in s_addr;
	int sock;
	int addr_len;
	int len;
	char buff1[128], buff2[128];
	struct iovec iov[2];
	struct msghdr msg;

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

	msg.msg_name = &s_addr;
	msg.msg_namelen = sizeof(struct sockaddr_in);
	msg.msg_iov = &iov[0];
	msg.msg_iovlen = 2;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	strcpy(buff1, "helloworldabcd");
	strcpy(buff2, "xxxxhelloworldabcd");

	iov[0].iov_base = buff1;
	iov[0].iov_len = strlen(buff1);
	iov[1].iov_base = buff2;
	iov[1].iov_len = strlen(buff2);

	len = sendmsg(sock, &msg, 0);
	if (len < 0) {
		fprintf(stdout, "send error %s\n", strerror(errno));
		return -1;
	}

	printf("send succes\n");

	return 0;
} 
