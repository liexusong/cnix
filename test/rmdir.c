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

	if(rmdir(argv[1]) < 0)
		printf("rmdir failed\n");

	return 0;
}
