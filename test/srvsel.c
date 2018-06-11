#include <stdlib.h> 
#include <stdio.h> 
#include <unistd.h> 
#include <sys/time.h> 
#include <sys/types.h> 
#include <string.h> 
#include <signal.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <errno.h> 
	
#define MAXCLIENT 10 
#define PORT 8888
#define BUF_SIZE 15
#undef max 
#define max(x,y) ((x) > (y) ? (x) : (y)) 

struct sockaddr_in clients[MAXCLIENT];

int resend_msg(int fd, struct sockaddr_in * clientaddr)
{
	int rec;
	char buffer[BUF_SIZE];

	memset(&buffer, 0, sizeof(buffer));

	rec = recv(fd, buffer, BUF_SIZE - 1, 0);
	if(rec < 0)
	{ 
		fprintf(stderr,
			"received from client: %s failed\n",
			inet_ntoa(clientaddr->sin_addr)); 
		exit(1); 
	} 
	else if(rec == 0)
	{
		return -1;
	}
	else 
	{ 
		buffer[rec] = '\0';

		fprintf(stdout,"from client(%s): %s\n",
			inet_ntoa(clientaddr->sin_addr), buffer);

		if(send(fd, buffer, rec, 0) != rec)
		{ 
			fprintf(stderr,
				"sendto client(%s) failed\n",
				inet_ntoa(clientaddr->sin_addr));
			return -1;
		}
	}

	return 0;
}

int main(int argc,char *argv[]) 
{ 
	int i, n; 
	fd_set rd;
	int listen_id;
	int fd[MAXCLIENT]; 
	struct sockaddr_in serveraddr;

	for(i = 0; i < MAXCLIENT; i++) 
	{ 
		fd[i] = -1;
	} 

	listen_id = socket(AF_INET, SOCK_STREAM, 0); 
	if(listen_id < 0) 
	{ 
		fprintf(stderr,"Create listen socket failure.\n"); 
		exit(1); 
	} 
	
	memset(&serveraddr, 0, sizeof(serveraddr)); 
	serveraddr.sin_family = AF_INET;             
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);   
	serveraddr.sin_port = htons(PORT);   
	
	if(bind(listen_id,
		(struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) 
	{ 
		fprintf(stderr,"bind listen socket failure.\n"); 
		exit(1); 
	} 
	
	if(listen(listen_id, MAXCLIENT) < 0) 
	{ 
		fprintf(stderr,"listen the socket failure.\n"); 
	} 
	
	while(1) 
	{ 
		int ret, nfds = listen_id;

		FD_ZERO(&rd); 
		FD_SET(listen_id, &rd);

		for(i = 0; i < MAXCLIENT; i++)
		{ 
			if(fd[i] > 0){
				FD_SET(fd[i], &rd);

				if(fd[i] > nfds)
					nfds = fd[i];
			}
		}	

		nfds = max(nfds, listen_id);

		ret = select(nfds + 1, &rd, NULL, NULL, NULL);
		if(ret < 0) 
		{ 
			if(EINTR == errno)
			{
				continue;
			} 
			else
			{
				fprintf(stderr,"begin select failure.\n");
			}
		}

		for(i = 0; i < MAXCLIENT; i++)
		{
			if((fd[i] < 0) || (fd[i] == listen_id))
			{
				continue;
			}

			if(FD_ISSET(fd[i], &rd))
			{
				if(resend_msg(fd[i], &clients[i]) < 0)
				{
					close(fd[i]);
					fd[i] = -1;
				}
			}
		}

		if(FD_ISSET(listen_id, &rd))
		{
			int client;
			struct sockaddr_in clientaddr;
			int addr_len = sizeof(struct sockaddr_in);

			memset(&clientaddr, 0, sizeof(clientaddr));

			client = accept(listen_id,
				(struct sockaddr *)&clientaddr, &addr_len);
			if(client < 0)
			{
				fprintf(stderr,
					"accept a new connect failure\n");
				exit(1);
			}
			else
			{
				for(i = 0; i < MAXCLIENT; i++)
					if(fd[i] < 0)
						break;

				if(i >= MAXCLIENT)
					close(client);
				else{
					fd[i] = client;
					clients[i] = clientaddr;
				}
			}
		}

	}
}
