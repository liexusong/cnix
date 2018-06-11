#ifndef _TIMES_H
#define _TIMES_H

#include <cnix/types.h>

/* Structure describing CPU time used by a process and its children.  */
struct tms{
	clock_t tms_utime;		/* User CPU time.  */
	clock_t tms_stime;		/* System CPU time.  */
	clock_t tms_cutime;		/* User CPU time of dead children.  */
	clock_t tms_cstime;		/* System CPU time of dead children.  */
};

extern clock_t times (struct tms * buff);

#endif
