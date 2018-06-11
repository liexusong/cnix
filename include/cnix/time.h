#ifndef _TIME_H
#define _TIME_H

#include <cnix/types.h>

struct timeval{
	time_t tv_sec;
	suseconds_t tv_usec;
};

struct timezone{
	int tz_minuteswest;
	int tz_dsttime;
};

struct utimbuf 
{
	time_t actime;
	time_t modtime;
};

#endif
