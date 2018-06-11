#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char * argv[])
{
	if(argc < 3){
		printf("kill pid signal\n");
		exit(-1);
	}

	kill(atoi(argv[1]), atoi(argv[2]));

	return 0;
}
