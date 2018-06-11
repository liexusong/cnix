
#ifndef SETJMP_H
#define SETJMP_H

#include <signal.h>

#define JMP_BUFF_SIZE 10
typedef unsigned int jmp_buf[JMP_BUFF_SIZE];

struct __sigjmp_buffer_struct
{
	jmp_buf __jmp_buffer;
	int	__save;
	sigset_t __mask;
};

typedef struct __sigjmp_buffer_struct sigjmp_buf[1];

extern void longjmp(jmp_buf buff, int val);
extern int setjmp(jmp_buf buff);

#define sigsetjmp(buf, save)	\
	(			\
		(buf->__save = save), \
		(save ? sigprocmask(SIG_SETMASK, NULL, &buf->__mask):0),\
			setjmp((unsigned int *)buf) 	\
	)

#define siglongjmp(buf, val)  \
	do {		\
		if (buf->__save)	\
		{				\
			sigprocmask(SIG_SETMASK,&buf->__mask, NULL);	\
		} \
		longjmp((unsigned int *)buf, val);		\
	} while (0)
		

#endif
