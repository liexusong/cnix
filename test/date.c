#include <stdio.h>
#include <time.h>

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

int main(int argc, char * argv[])
{
	time_t seconds = time(NULL);
	struct bios_time time;

	second_to_time(seconds, &time);

	printf(
		"Date: %04d-%02d-%02d Time: %02d:%02d:%02d\n",
		time.year, time.month, time.day,
		time.hour, time.minute, time.second
		);

	return 0;
}
