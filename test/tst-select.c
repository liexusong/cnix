#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "sys/select.h"


//static struct termios backup_term;

enum
{
	RESET,
	RAW,
	CBREAK
};

int tty_state = RESET;
int saved_fd = -1;

#if 0
///
static int tty_raw(int fd)
{
	struct termios tm;
	
	if (tcgetattr(0, &backup_term) < 0)
	{
		return -1;
	}

	tm = backup_term;

	tm.c_lflag &= ~(ICANON | ECHO | IEXTEN | ISIG);
	tm.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	tm.c_cflag &= ~(CSIZE | PARENB);
	tm.c_cflag |= CS8;
	tm.c_oflag &= ~(OPOST);
	
	tm.c_cc[VMIN] = 1;
	tm.c_cc[VTIME] = 0;

	if (tcsetattr(0, TCSAFLUSH, &tm) < 0)
	{
		return -1;
	}

	saved_fd = fd;
	tty_state = RAW;

	return 0;
	
}

///
static int tty_reset(int fd)
{
	if (tty_state != CBREAK && tty_state != RAW)
	{
		return 0;
	}

	if (tcsetattr(fd, TCSAFLUSH, &backup_term) < 0)
	{
		return -1;
	}

	tty_state = RESET;

	return 0;
}
#endif

int main(void)
{
	fd_set rs;
	fd_set ws;
	fd_set es;
	int maxfd;
	int ret;
	int c;
	struct timeval val;
	
	//tty_raw(0);

	maxfd = 3;


	for (;;)
	{
		FD_ZERO(&rs);
		FD_ZERO(&ws);
		FD_ZERO(&es);

		FD_SET(0, &rs);
		FD_SET(1, &rs);

		FD_SET(1, &es);
		FD_SET(2, &es);
		val.tv_sec = 2;
		val.tv_usec = 100;

		printf("begin select\n");

		if ((ret = select(maxfd, &rs, &ws, &es, &val)) < 0)
		{
			printf("select error(%d) = %s!\n", errno, strerror(errno));
		}
		else if (ret == 0)
		{
			printf("timeout!\n");
		}
		else
		{
			printf("has something!\n");

			read(0, &c, 1);
			printf("c = %c", c);

			if (c == 'q')
			{
				break;
			}
		}
	}

	//tty_reset(0);

	return 0;
}
