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
	int sock, fd;
	int len;
	socklen_t addr_len;
	char buff[15];

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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
	}

	listen(sock, 5);

	while ((fd = accept(sock,
		(struct sockaddr *)&c_addr, &addr_len)) > 0) {
		len = read(fd, buff, sizeof(buff) - 1);
		if (len < 0) {
			fprintf(stdout, "read error %s\n", strerror(errno));
			return -1;
		}

		buff[len] = '\0';

		printf(
			"%s %d %s\n",
			inet_ntoa(c_addr.sin_addr),
			ntohs(c_addr.sin_port),
			buff
			);

		close(fd);
	}

	close(sock);

	return 0;
}
