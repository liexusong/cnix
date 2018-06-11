#include <stdio.h>
#include <unistd.h>

int main(int argc, char * argv[])
{
	char * p = (char *)0x200000;

	*p = 'a';

	_exit(0);

	return  0;
}
