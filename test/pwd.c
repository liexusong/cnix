#include <stdio.h>
#include <unistd.h>

int main(void)
{
	char buffer[1024];

	if(getcwd(buffer, 1024) < 0)
		printf("getcwd error\n");
	else
		printf("%s\n", buffer);

	return 0;
}
