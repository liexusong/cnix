#ifndef _SELECT_H
#define _SELECT_H

#include <cnix/wait.h>

#define TYPE_REG_FILE	0x01	// Normal disk file, needn't to wait
#define TYPE_TTY	0x02	// tty, need to read, no need to write
#define TYPE_SOCK	0x04	// socket,
#define TYPE_PIPE	0x08	// pipe,

#define OPTYPE_READ	0x01
#define OPTYPE_WRITE	0x02
#define OPTYPE_EXCEPT	0x04

struct fd_type 
{
	short type;
	short optype;
	int fd;
	void * type_ptr;
};

typedef struct select_struct{
	short optype;
	struct wait_queue ** wait_spot;
}select_t;

extern void select_init(select_t * sel);
extern void select_wakeup(select_t * sel, short optype);

#endif
