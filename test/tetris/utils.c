/* the file which is used for debugging */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include <stdlib.h>
#include <fcntl.h>

#include "utils.h"



static debug_file_t dfile = {-1,};

///
debug_file_t *new_debug_file(const char *name, int append)
{
	int fd;
	char err_msg[] = "open name failed!\n";

	dfile.file_fd = -1;
	
	if (append)
	{
		fd = open(name, O_RDWR | O_CREAT | O_APPEND, 0700);
	}
	else 
	{
		fd = open(name, O_WRONLY | O_CREAT, 0700);
	}

	if (fd < 0)
	{
		write(2, err_msg, strlen(err_msg));
		exit(1);
	}

	dfile.file_fd = fd;
	strcpy(dfile.file_name, name);

	return &dfile;
}

///
int close_debug_file(debug_file_t *file_ptr)
{
	if (!file_ptr || file_ptr->file_fd < 0)
	{
		return -1;
	}

	if (close(file_ptr->file_fd) < 0)
	{
		return -1;
	}

	return 0;
}

#if 0

/// help function
int sprintf(char *buff, const char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = vsprintf(buff, fmt, args);
	va_end(args);

	return n;	
}
#endif


/// level 
int dprint(debug_file_t * file_ptr,  int level, const char *fmt, ...)
{
	char buff[64];		// is it too large?
	va_list args;
	int len;
	int i;
	
	if (!file_ptr || file_ptr->file_fd < 0)
	{
		return -1;
	}
	
	/* add <level> : to the begin */
	i = 0;
	buff[i++] = '<';
	sprintf(&buff[i], "%d", level);
	i = strlen(buff);
	buff[i++] = '>';
	buff[i++] = ':';
	
	va_start(args, fmt);
	vsprintf(&buff[i], fmt, args);
	va_end(args);

	len = strlen(buff);
	
	return write(file_ptr->file_fd, buff, len);
}


/* rand function code: from isdn */

#define RANDOM_MAX 0x7FFFFFFF

static long _my_do_rand(unsigned long *value)

{
   long quotient, remainder, t;

   quotient = *value / 127773L;
   remainder = *value % 127773L;
   t = 16807L * remainder - 2836L * quotient;

   if (t <= 0)
      t += 0x7FFFFFFFL;

   return ((*value = t) % ((unsigned long)RANDOM_MAX + 1));
}

static unsigned long next = 1;

int my_rand(void)
{
   return _my_do_rand(&next);
}

void my_srand(unsigned int seed)
{
   next = seed;
}

/* rand code end */

#define BUFF_SIZE 1024
#define WORK_SIZE 128

static char my_buffer[BUFF_SIZE];
static int buffer_index = 0;
static char work_buffer[WORK_SIZE];

int buffer_printf(const char *fmt, ...)
{
	int len;
	va_list args;
	int n = BUFF_SIZE - buffer_index;


	va_start(args, fmt);
	len = vsprintf(work_buffer, fmt, args);
	va_end(args);

	if (len > n)	// there ins't enough buffer to hold the string.
	{
		memcpy(&my_buffer[buffer_index], &work_buffer[0], n);

		// flush to screen
		write(1, my_buffer, BUFF_SIZE);

		buffer_index = 0;
		len -= n;
	}
	else
	{
		n = 0;
	}

	memcpy(&my_buffer[buffer_index], &work_buffer[n], len);	

	buffer_index += len;

	return (len + n);
}


void buffer_fflush(void)
{
	//return;
	// flush to screen
	write(1, my_buffer, buffer_index);
	buffer_index = 0;
}
