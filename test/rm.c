#include <stdio.h>
#include <unistd.h>

int main(int argc, char * argv[])
{
	if(argc != 2){
		printf("%s file\n", argv[0]);

		_exit(0);
	}

	if(unlink(argv[1]))
		printf("%s not exist\n", argv[1]);
	
	return 0;
}
