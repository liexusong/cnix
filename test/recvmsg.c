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
	struct sockaddr_in c_addr;
	int sock;
	int len;
	char buff1[20], buff2[128];
	struct iovec iov[2];
	struct msghdr msg;


	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stdout, "socket error %s\n", strerror(errno));
		return -1;
	} else
		printf("create socket\n");

	memset(&s_addr, 0, sizeof(struct sockaddr_in));

	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(8888);
	s_addr.sin_addr.s_addr = INADDR_ANY;

	if ((bind(sock, (struct sockaddr *) &s_addr, sizeof(s_addr))) < 0) {
		fprintf(stdout, "bind error %s\n", strerror(errno));
		return -1;
	} else
		printf("bind address to socket");

	msg.msg_name = &c_addr;
	msg.msg_namelen = sizeof(struct sockaddr_in);
	msg.msg_iov = &iov[0];
	msg.msg_iovlen = 2;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	iov[0].iov_base = buff1;
	iov[0].iov_len = sizeof(buff1) - 1;
	iov[1].iov_base = buff2;
	iov[1].iov_len = sizeof(buff2) - 1;

	while (1) {
		len = recvmsg(sock, &msg, 0);
		if (len < 0) {
			fprintf(stdout, "recvfrom error %s\n", strerror(errno));
			return -1;
		}

		if(len > sizeof(buff1) - 1)
			buff1[iov[0].iov_len] = '\0';

		if(len > sizeof(buff1) - 1 + sizeof(buff2) - 1)
			buff2[iov[1].iov_len] = '\0';

		printf(
			"%s %d %s%s\n",
			inet_ntoa(c_addr.sin_addr),
			ntohs(c_addr.sin_port),
			buff1, buff2
			);
	}

	return 0;
}
