#ifndef _TIMER_H
#define _TIMER_H

#include <cnix/types.h>
#include <cnix/list.h>

typedef void (*timerproc_t)(void *);

/* syncronous timer */
typedef struct synctimer_struct{
	list_t list;
	unsigned long expire;
	void * data;
	timerproc_t timerproc;
}synctimer_t;

/* asyncronous timer */
typedef struct asynctimer_struct{
	list_t list;
	unsigned long expire;
}asynctimer_t;

extern void synctimer_set(synctimer_t *);
extern void asynctimer_set(asynctimer_t *);
extern inline void sync_delete(synctimer_t * sync);
extern inline void async_delete(asynctimer_t * async);
	
typedef struct mstate_struct{
	unsigned long prev;
	unsigned long now;
}mstate_t;

extern void mstart(mstate_t *);
extern unsigned long melapsed(mstate_t *);
extern void mdelay(unsigned long);

extern time_t curclock(void);
extern unsigned long utc_time_diff(void);

extern unsigned long volatile nowticks;

#endif
