#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char * argv[])
{
	if(argc < 2){
		printf("%s pathname\n", argv[0]);
		_exit(0);
	}

	if(mkdir(argv[1], 0755) < 0)
		printf("mkdir failed\n");

	return 0;
}
