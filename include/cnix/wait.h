#ifndef _WAIT_H
#define _WAIT_H

#include <cnix/types.h>

struct wait_queue{
	struct task_struct * task;
	struct wait_queue * next;
};

static inline void wait_init(struct wait_queue ** wq)
{
	if(wq)
		*wq = NULL;
}

#endif
