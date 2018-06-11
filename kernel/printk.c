#include <cnix/string.h>
#include <cnix/stdarg.h>

extern int vsprintf(char *buf, const char *fmt, va_list args);
extern void kputs(char *s);

static char buf[1024];

int printk(const char * fmt, ...)
{
	int len;
	va_list args;

	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);

	kputs(buf);

	return len;
}

int sprintf(char * buff, const char * fmt, ...)
{
	int len;
	va_list args;

	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);

	strcpy(buff, buf);

	return len;
}

void panic(const char * fmt, ...)
{
	va_list args;

	kputs("panic: ");

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	kputs(buf);

	for(;;);
}
