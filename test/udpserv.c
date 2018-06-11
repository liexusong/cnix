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
	struct sockaddr_in c_addr;
	int sock;
	socklen_t addr_len;
	int len;
	char buff[128];

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
		close(sock);
		fprintf(stdout, "bind error %s\n", strerror(errno));
		return -1;
	}

	addr_len = sizeof(c_addr);

	while (1) {
		len = recvfrom(sock, buff, sizeof(buff) - 1, 0,
			(struct sockaddr *) &c_addr, &addr_len);
		if (len < 0) {
			fprintf(stdout, "recvfrom error %s\n", strerror(errno));
			return -1;
		}

		buff[len] = '\0';
		printf(
			"%s %d %s\n",
			inet_ntoa(c_addr.sin_addr),
			ntohs(c_addr.sin_port),
			buff
			);
	}

	close(sock);

	return 0;
}
