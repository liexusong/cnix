#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

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

void display_time(void)
{
	time_t seconds = time(NULL);
	struct bios_time time;

	second_to_time(seconds, &time);

	printf("\0337\033[1;60H\033[35m");
	printf(
		"%04d-%02d-%02d %02d:%02d:%02d",
		time.year, time.month, time.day,
		time.hour, time.minute, time.second
		);
	printf("\033[0m\0338");
	fflush(stdout);
}

int daemon_init(void)
{
	pid_t pid;

	if((pid = fork()) < 0)
		return -1;
	else if(pid != 0)
		exit(0);

	//setsid();

	chdir("/");

	//umask(0);

	return 0;
}

void sigalrm(int signum)
{
	signal(SIGALRM, sigalrm);
}

int main(void)
{
	if(!daemon_init()){
		signal(SIGALRM, sigalrm);

		for(;;){
			alarm(1);
			pause();
			display_time();
		}
	}

	printf("daemon_init failed\n");

	return 0;
}
