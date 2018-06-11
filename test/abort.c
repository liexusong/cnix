#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(void)
{
	sleep(10);
	abort();
	printf("cannot happen\n");
	return 0;
}

