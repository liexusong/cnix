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
	char buff[15];

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stdout, "socket error %s\n", strerror(errno));
		exit(-1);
	}

	s_addr.sin_family = AF_INET;

	s_addr.sin_port = htons(8888);
	if(argc >= 3)
		s_addr.sin_port = htons(atoi(argv[2]));


	if(argc >= 2)
		s_addr.sin_addr.s_addr = inet_addr(argv[1]);
	else
		s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	addr_len = sizeof(s_addr);

	if(connect(sock, (struct sockaddr *)&s_addr, addr_len) < 0){
		close(sock);
		fprintf(stderr, "connect error %s\n", strerror(errno));
		return -1;
	}

	strcpy(buff, "helloworldabcd");

	len = write(sock, buff, strlen(buff));
	if (len < 0) {
		close(sock);
		fprintf(stdout, "send error %s\n", strerror(errno));
		return -1;
	}

	len = read(sock, buff, len);
	if(len > 0){
		if(len > 14)
			len = 14;
		buff[len] = '\0';
		printf("get a message %s\n", buff);
	}else
		printf("send failed\n");

	close(sock);

	return 0;
} 
