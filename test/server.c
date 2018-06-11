#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "hdr.h"

char buff[BUFF_SIZE];

void append_to_file(char *ptr, int len);

int main(int argc, char *argv[])
{
	struct sockaddr_in s_addr;
	struct sockaddr_in c_addr;
	int sock;
	int addr_len;
	int len;

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("socket error: %s\n", strerror(errno));
		return 1;
	}

	memset(&s_addr, 0, sizeof(struct sockaddr_in));
	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(8888);
	s_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0)
	{
		printf("bind error %s\n", strerror(errno));
		return -1;
	}
	
	addr_len = sizeof(c_addr);
	
	while (1)
	{
		len = recvfrom(sock, buff, sizeof(buff), 0,
			(struct sockaddr *)&c_addr, &addr_len);

		if (len < 0)
		{
			printf("recv error %s\n", strerror(errno));
			fflush(stdout);

			return -1;
		}	
		
		printf("%s %d\n",
			inet_ntoa(c_addr.sin_addr),
			ntohs(c_addr.sin_port));
#if 0
		{
			int i;

			for (i = 0; i < len; i++)
			{
				printf("%c", buff[i]);
			}
		}
#endif
		append_to_file(buff, len);
		printf("len = %d\n", len);
		fflush(stdout);

	}
	
	close(sock);	
	
	return 0;
}

void append_to_file(char *ptr, int len)
{
	FILE *fp;
	int size;

	fp = fopen("a.c", "wb+");
	
	if (!fp)
	{
		printf("open for a.c failed!\n");
		return;
	}
	
	size = fwrite(ptr, 1, len, fp);
	if (size != len)
	{
		printf("fwrite error?\n");
	}
	
	fclose(fp);	
	
}

