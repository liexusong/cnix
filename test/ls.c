#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>

struct bios_time{
	int second;
	int minute;
	int hour;
	int day;
	int month;
	int year;
	int century;
};

#define MINUTE	60
#define HOUR	(MINUTE * 60)
#define DAY	(HOUR * 24)
#define YEAR	(DAY * 365)

/* the months of leap year */
static int month[12] = {
	0,
	DAY * (31),
	DAY * (31 + 29),
	DAY * (31 + 29 + 31),
	DAY * (31 + 29 + 31 + 30),
	DAY * (31 + 29 + 31 + 30 + 31),
	DAY * (31 + 29 + 31 + 30 + 31 + 30),
	DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31),
	DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31),
	DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30),
	DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31),
	DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30),
};

#define leapyear(year) ((!(year % 400)) || (!(year % 4) && (year % 100)))

static void second_to_time(unsigned long seconds, struct bios_time * time)
{
	int year;
	unsigned long year_sec, secbak;

	for(year = 1970; ; year++){
		secbak = seconds;

		seconds -= YEAR;
		year_sec = YEAR;
		if(leapyear(year)){
			seconds -= DAY;
			year_sec += DAY;
		}

		if(seconds > secbak){
			seconds += year_sec;
			break;
		}
	}

	time->year = year;

	{
		int i;

		if(leapyear(year))
			month[2] = DAY * (31 + 29);
		else
			month[2] = DAY * (31 + 28);
			
		for(i = 1; i < 12; i++)
			if(seconds < month[i])
				break;

		time->month = i;

		seconds -= month[i - 1];
		time->day = seconds / DAY + 1;

		seconds -= (time->day - 1) * DAY;
		time->hour = seconds / HOUR;

		seconds -= time->hour * HOUR;
		time->minute = seconds / MINUTE;

		seconds -= time->minute * MINUTE;
		time->second = seconds;
	}
}

void verboseit(struct stat * st)
{
	int i;
	char * mode = "rwx";

	for(i = 8; i >= 0; i--)
		if(st->st_mode & (1 << i)){
			printf("%c", mode[i % 3]);
		}else
			printf("-");

	printf("\t");

	printf("%d\t", st->st_nlink);

	if(st->st_uid == 0)
		printf("root\t");
	else
		printf("%d\t", st->st_uid);

	if(st->st_gid == 0)
		printf("root\t");
	else
		printf("%d\t", st->st_uid);

	printf("%ld\t", st->st_size);

	{
		struct bios_time time;

		second_to_time(st->st_mtime, &time);

		printf("%04d-%02d-%02d-%02d-%02d-%02d\t",
			time.year, time.month, time.day,
			time.hour, time.minute, time.second
			);
	}
}

int main(int argc, char * argv[])
{
	int fd = 0; // avoid warning
	char name[32];
	struct dirent dir;
	struct stat st;
	int verbose = 0;

	if(argc < 2)
		fd = open(".", O_RDONLY);
	else if(argc == 2){
		if(strcmp(argv[1], "-l") == 0){
			verbose = 1;
			fd = open(".", O_RDONLY);
		}else
			fd = open(argv[1], O_RDONLY);	
	}else if(argc == 3){
		if(strcmp(argv[1], "-l") == 0)
			verbose = 1;
		else{
			printf("ls [-l] dirname\n");
			exit(0);
		}

		if(stat(argv[2], &st) < 0){
			printf("error: %s\n", strerror(errno));
			exit(-1);
		}

		if(S_ISREG(st.st_mode)){
			verboseit(&st);
			printf("\n");
			exit(0);
		}

		fd = open(argv[2], O_RDONLY);
	}

	if(fd < 0){
		printf("open dir error\n");
		exit(0);
	}

	while (read(fd, (char *)&dir, 32) > 0){
		if (dir.d_ino == 0)
			continue;

		strncpy(name, dir.d_name, 30);
		name[30] = '\0';

		if (strcmp(name, ".") != 0  && strcmp(name, "..") != 0){
			memset(&st, 0, sizeof(stat));

			if (stat(name, &st) >= 0){
				if(verbose)
					verboseit(&st);

				if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode))
					printf("\033[33m");
				else if (S_ISDIR(st.st_mode))
					printf("\033[34m");
				else if (S_ISREG(st.st_mode)){
					if ((st.st_mode & I_XB)
						|| ((st.st_mode >> 3) & I_XB)
						|| ((st.st_mode >> 6) & I_XB))
							printf("\033[32m");
				}
			}
		
			printf("%s\033[0m\n", name);
		}
	}

	return 0;
}
