#ifndef ED_H
#define ED_H

#include "misc.h"

#define MAX_FILENAME	256
#define MAX_PROMPT_SIZE	16


typedef struct _line_buffer_t
{
	char *line_ptr;
	int   line_size;
	int   refcnt;
} line_buffer_t;

#define line_ptr	line_buffer.line_ptr
#define line_size	line_buffer.line_size
#define line_refcnt	line_buffer.refcnt
#define INC_REFCNT(line)	(line->line_buffer.refcnt++)
#define DEC_REFCNT(line)	(line->line_buffer.refcnt--)

typedef struct _line_t 
{
/*
	char *line_ptr;		// orignal line 
	int   line_size;
*/
	line_buffer_t  line_buffer;

	struct _line_t *line_next;
	struct _line_t *line_prev;
} line_t;

typedef struct _undo_node_t
{
	int	cmd;

	line_t *undo_line_begin;
	line_t *undo_line_end;

	// 
	int	undo_index_begin;
	int	undo_line_count;
} undo_node_t;

struct _file_buffer
{
	char filename[MAX_FILENAME];
	char prompt_string[MAX_PROMPT_SIZE];
	line_t *head;
	line_t *tail;

	undo_node_t *undo_node;

	BOOLEAN	dirty;		//
	BOOLEAN  show_error;
	BOOLEAN  show_prompt;
	
	int	error_code;
	unsigned long filesize;
	int current_address;
	int end_address;
};

typedef struct _file_buffer file_buffer_t;

typedef line_t  *(*cmd_func_t)(line_t *, line_t *);

typedef struct _cmd_table 
{
	int 		cmd_char;
	cmd_func_t 	func;
} cmd_table_t;

enum 
{
	ERR_OK,
	ERR_FILE_OPEN,
	ERR_ADDRESS_INVALID,
	ERR_FILE_MODIFIED,
	ERR_REGEXP_INVALID,
	ERR_UNKOWN_CMD,
	ERR_NUM
};


#define LOG_ERROR1(f, s)	do { \
					debug_print(__FILE__, __LINE__, f, s); \
					exit(1);	\
				} while(0)
#define LOG_ERROR2(f, s1, s2)	do { \
					debug_print(__FILE__, __LINE__, f, s1, s2); \
					exit(1);	\
				} while(0)
#define LOG_ERROR3(f, s1, s2, s3)	do { \
					debug_print(__FILE__, __LINE__, f, s1, s2, s3); \
					exit(1);	\
				} while(0)

#define UNREFERENCED_PARAM(x)		x = x


extern int current_address;

extern int end_address;


line_t *cmd_append(file_buffer_t * filep, int nr_begin, int nr_end);

line_t *cmd_delete(file_buffer_t * filep, int begin, int end);

line_t *cmd_insert(file_buffer_t * filep, int begin, int end);

BOOLEAN cmd_quit(file_buffer_t *filep,int begin, int end);

//
void cmd_write(file_buffer_t *filep, int begin, int end);

int get_line_index(line_t *head, line_t *node);

line_t *get_line(line_t *head, int index);

line_t *cmd_print(file_buffer_t * filep, int nr_begin, int nr_end, BOOLEAN show_nr);

line_t * cmd_undo(file_buffer_t *filep);

void free_lines(line_t *header);

#endif
