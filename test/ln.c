#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int main(int argc, char * argv[])
{
	if(argc < 3){
		printf("ln src dest\n");
		exit(-1);
	}

	if(link(argv[1], argv[2]) < 0){
		printf("error(%d): %s\n", errno, strerror(errno));
		exit(-1);
	}

	return 0;
}
