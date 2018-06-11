#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

volatile int timeout = 0;
int bugstack[10000];

void alarm_handler(int signum)
{
	int i;

	timeout = 1;
	for(i = 0; i < 10000; i++)
		bugstack[i] = i + signum;
}

struct termios stOldTerm;

void settty(int iFlag, int level)
{
	struct termios stTerm;

	if(iFlag == 1){
		tcgetattr(0, &stTerm);

		stOldTerm = stTerm;

		stTerm.c_lflag &= ~ICANON;
		stTerm.c_lflag &= ~ECHO;
		stTerm.c_cc[VTIME] = 10 - level;
		stTerm.c_cc[VMIN] = 0;
		stTerm.c_iflag &= ~ISTRIP;
		stTerm.c_cflag |= CS8;
		stTerm.c_cflag &= ~PARENB;

		tcsetattr(0, TCSETA, &stTerm);
	}else
		tcsetattr(0, TCSETA, &stOldTerm);
}

int gint, gint1;

int main(int argc, char * argv[])
{
	for(;;){
		signal(SIGALRM, alarm_handler);
		alarm(1);
		while(!timeout){
			int * x = &gint, * y = &gint1;

			(*x)++;
			*y = *x;
		}
		timeout = 0;
		printf("Alarm ... %d\n", gint1);
		gint = 0;
		fflush(stdout);
	}

	printf("signal successfully\n");

	return 0;
}
