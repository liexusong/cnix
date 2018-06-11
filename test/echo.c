#include <stdio.h>
#include <unistd.h>

int main(int argc, char * argv[])
{
	if(argc != 2){
		printf("%s string\n", argv[0]);
		_exit(0);
	}

	printf("%s\n", argv[1]);

	return  0;
}
