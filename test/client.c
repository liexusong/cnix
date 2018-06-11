#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "hdr.h"

char buff[BUFF_SIZE];
int fill_buffer(char *buffer, int size, int pos);

int main(int argc, char * argv[])
{
	struct sockaddr_in s_addr;
	int sock;
	int addr_len;
	int len;
	int pos;
	int ret;

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stdout, "socket error %s\n", strerror(errno));
		exit(-1);
	}

	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(8888);
	if(argc >= 2)
		s_addr.sin_addr.s_addr = inet_addr(argv[1]);
	else
		s_addr.sin_addr.s_addr = inet_addr("192.168.1.123");

	addr_len = sizeof(s_addr);

	pos = 0;

	for (;;)
	{
		len = fill_buffer(buff, sizeof(buff), pos);
		pos += len;
		ret = sendto(sock, buff, len, 0,
			(struct sockaddr *) &s_addr, addr_len);

		if (ret < 0)
			fprintf(stdout, "send error %s\n", strerror(errno));
		else
			printf("send succes\n");

		if (len < sizeof(buff))
		{
			break;	// have read all the file.
		}
	}

	close(sock);

	return 0;
}

int fill_buffer(char *buffer, int size, int pos)
{
	FILE *fp;
	int len;
	
	fp = fopen("a.c", "rb");
	if (!fp)
	{
		printf("open for a.c reading failed.!\n");
		return -1;
	}

	fseek(fp, pos, SEEK_SET);

	len = fread(buffer, 1, size, fp);

	fclose(fp);

	return len;
}
