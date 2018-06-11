#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	struct stat statbuf;

	if (argc < 2)
	{
		printf("tst-stat file\n");
		exit(1);
	}

	if (stat(argv[1], &statbuf) < 0)
	{
		printf("stat failed!\n");
		exit(1);
	}

	if (S_ISDIR(statbuf.st_mode))
	{
		printf("%s is dir\n", argv[1]);
	}
	else if (S_ISREG(statbuf.st_mode))
	{
		printf("%s is regular file.\n", argv[1]);
	}
	else if (S_ISCHR(statbuf.st_mode))
	{
		printf("%s is char device.\n", argv[1]);
	}
	else if (S_ISBLK(statbuf.st_mode))
	{
		printf("%s is blk device.\n", argv[1]);
	}

	printf("st_dev =\t%ld\n", statbuf.st_dev);
	printf("st_inode =\t%ld\n", statbuf.st_ino);
	printf("st_nlink =\t%d\n", statbuf.st_nlink);
	printf("st_uid =\t%d\n", statbuf.st_uid);
	printf("st_gid =\t%d\n", statbuf.st_gid);
	printf("st_rdev =\t%ld\n", statbuf.st_rdev);
	printf("st_size =\t%ld\n", statbuf.st_size);
	printf("st_blksize =\t%ld\n", statbuf.st_blksize);
	printf("st_blocks =\t%ld\n", statbuf.st_blocks);
	printf("st_atime =\t%ld\n", statbuf.st_atime);
	printf("st_mtime =\t%ld\n", statbuf.st_mtime);
	printf("st_ctime =\t%ld\n", statbuf.st_ctime);

	return 0;
}
