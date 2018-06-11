#include <stdio.h>
#include <stdlib.h>

int main(int argc, char * argv[], char * envp[])
{
	if(argc < 2){
		printf("env envname\n");
		return 0;
	}

	printf("env %s=%s\n", argv[1], getenv(argv[1]));

	return 0;
}
