#include <stdio.h>
#include <unistd.h>
#include <sys/statfs.h>

int main(int argc, char *argv[])
{
	struct statfs stat;

	if (argc != 2)
	{
		printf("%s path\n", argv[0]);
		return 1;
	}

	if (statfs(argv[1], &stat) < 0)
	{
		printf("statfs error!\n");
		return 2;
	}

	printf("fs type = %x\n", (int)stat.f_type);
	printf("fs bsize = %d\n", (int)stat.f_bsize);
	printf("fs blocks = %d\n", (int)stat.f_blocks);
	printf("fs bfree = %d\n", (int)stat.f_bfree);
	printf("fs bavail = %d\n", (int)stat.f_bavail);
	printf("fs ffiles = %d\n", (int)stat.f_files);
	printf("fs ffree = %d\n", (int)stat.f_ffree);
	printf("fs namelen = %d\n", (int)stat.f_namelen);

	return 0;
}	


