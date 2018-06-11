#include <stdio.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>

int flag;

void handler(int signum)
{
	flag = 0;
	printf("exit...\n");
}

int main(void)
{
	struct termios oldt, t;	

	if(tcsetattr(0, TCSANOW, &oldt) < 0){
		printf("tcsetattr failed!\n");
		_exit(-1);
	}

	t = oldt;

	t.c_lflag &= ~ICANON;
	t.c_cc[VMIN] = 0;
	t.c_cc[VTIME] = 2;
	tcsetattr(0, TCSANOW, &t);

	flag = 1;

	signal(SIGINT, handler);

	do{
		char ch;

		ch = getchar();
		if(ch)
			printf("%x\n", ch);
	}while(flag);

	tcsetattr(0, TCSANOW, &oldt);

	return 0;
}
