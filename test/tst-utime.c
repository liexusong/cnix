#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <utime.h>
#include <string.h>

int main(int argc, char *argv[])
{
	struct utimbuf buf;
	struct tm t;
	time_t tmp;


	if (argc != 2)
	{
		printf("%s filename\n", argv[0]);
		return 1;
	}
	
	memset(&t, 0, sizeof(struct tm));
	t.tm_sec = 0;
	t.tm_min = 0;
	t.tm_hour = 0;
	t.tm_mday = 1;
	t.tm_mon = 0;
	t.tm_year = 2008 - 1900;

	tmp = mktime(&t);
	if (tmp == (time_t)-1)
	{
		printf("mktime error?!\n");
	}

	buf.actime = mktime(&t);
	buf.modtime = mktime(&t);

	if (utime(argv[1], &buf) < 0)
	{
		printf("utime error!\n");

		return 1;
	}

	return 0;	
}
