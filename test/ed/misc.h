#ifndef MISC_H
#define MISC_H

typedef enum 
{
	FALSE,
	TRUE
} BOOLEAN;

extern int debug_print(char *filename, int line_no, const char *fmt, ...);

#endif
