#ifndef UTILS_H
#define UTILS_H

#ifndef NAME_MAX
#define NAME_MAX 30
#endif

typedef struct _debug_file 
{
	int file_fd;
	char file_name[NAME_MAX];
} debug_file_t;

typedef enum 
{
	FALSE,
	TRUE
} BOOLEAN;

#ifdef CNIX
int sprintf(char *buff, const char *fmt, ...);
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

///
void my_srand(unsigned int seed);

///
int my_rand(void);

///
debug_file_t *new_debug_file(const char *name, int append);

///
int close_debug_file(debug_file_t *file_ptr);

///
int dprint(debug_file_t * file_ptr,  int level, const char *fmt, ...);

#endif
