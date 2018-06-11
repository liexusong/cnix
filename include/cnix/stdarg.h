#ifndef _STDARG_H
#define _STDARG_H

typedef char * va_list;

#define va_start(args, fmt) \
	(args = ((va_list)(&fmt + 1)))

#define va_end(args) 

#define va_arg(args, type) \
	(args += sizeof(va_list), *((type *)(args - sizeof(va_list))))

extern int vsprintf(char *, const char * fmt, va_list);

#endif
