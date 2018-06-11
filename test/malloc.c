#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	int i;

	for(i = 0; i < 100; i++)
		malloc(12);

	printf("malloc success\n");

	return 0;
}
