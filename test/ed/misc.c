#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "misc.h"

static int debug_fd = 2;

/// level 
int debug_print(char *filename, int line_no, const char *fmt, ...)
{
	char buff[64];		// is it too large?
	va_list args;
	int len;
	int i;

	
	/* add <level> : to the begin */
	i = 0;
	buff[i++] = '<';
	sprintf(&buff[i], "%s", filename);
	i = strlen(buff);
	buff[i++] = ':';
	sprintf(&buff[i], "%d", line_no);
	i = strlen(buff);
	buff[i++] = '>';
	buff[i++] = ':';
	
	va_start(args, fmt);
	vsprintf(&buff[i], fmt, args);
	va_end(args);

	len = strlen(buff);
	
	return write(debug_fd, buff, len);
}
