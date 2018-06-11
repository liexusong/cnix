#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>

#include "ed.h"


#define INIT_MEM_SIZE	12
#define INC_MEM_SIZE	4
#define BUFF_SIZE	128
#define isblank(c)	((c) == ' ' || (c) == '\t')

enum 
{
	STATE_INITIAL,
	STATE_GOT_NUMBER,
	STATE_GOT_ADD,
	STATE_GOT_SUB,
	STATE_INVALID
};

static const char *error_msg[] =
{
	"success.",
	"open file failed.",
	"invalid address.",
	"file is modified.",
	"regexp error",
	NULL
};


static const char cmd_chars[] = 
{
	'a', 'c', 'd', 'e', 'E',
	'f', 'g', 'G', 'H',
	'h', 'i', 'j', 'l', 'm', 'n', 'p', 
	'P', 'q', 'Q', 's', 't', 'u',
	'v', 'V', 'w', 'W', 'x',
	'y', 'z', '#', '!', '\0'
};


typedef void (*fn_t)(int row, char *ptr);


static void iterate_line_list(const line_t *header, fn_t f);


///
static int get_newline_index(const char *buf, int buf_size)
{
	int index;
	const char *p;
	
	for (index = 0, p = buf; index < buf_size; index++, p++)
	{
		if (*p == '\n')
		{
			return index;	
		}
	}

	return -1;
}

/*
 * There are many lines (> 1) in the last data chunk.
 */
static BOOLEAN check_newline_index(int fd, const char *buf, int buf_size, int *size)
{
	int index;
	
	if ((index = get_newline_index(buf, buf_size)) >= 0)
	{
		*size = index + 1;

		// go back to the last new line position 
		if (lseek(fd, -(buf_size - *size), SEEK_CUR) < 0)
		{
			LOG_ERROR2("lseek error errno = %d, %s\n", errno, strerror(errno));
		}

		return TRUE;
	}

	*size = buf_size;

	return FALSE;
}

///
static char *check_next_chunk(int fd, char *orig, int osize, int *nsize)
{
	int new_index;
	int nread;
	char *new_ptr;
	char *p;
	
	p = (char *)realloc(orig, osize + INC_MEM_SIZE);
	assert(p != NULL);

	new_ptr = p + osize;

	if ((nread = read(fd, new_ptr, INC_MEM_SIZE)) < 0)
	{
		// something error
		LOG_ERROR1("read error: %s\n", strerror(errno));
		free(p);
		
		return NULL;
	}
	else if (nread == 0)
	{
		// end of file
		*nsize = osize;
		p[*nsize] = '\0';
		return p;
	}
	else if (nread < INC_MEM_SIZE)
	{
		int size;
		
		check_newline_index(fd, new_ptr, nread, &size);
		
		// 
		*nsize = osize + size;
		p[*nsize] = '\0';

		return p;
	}
	else 
	{
		if ((new_index = get_newline_index(new_ptr, nread)) >= 0)
		{
			*nsize = osize + new_index + 1;

			if (new_index == INC_MEM_SIZE - 1)
			{
				// not enough memory to save '\0'
				p = realloc(p, *nsize + 1);
				assert(p != NULL);
			}

			p[*nsize] = '\0';

			// back the file ptr
			if (lseek(fd, -(nread - new_index - 1), SEEK_CUR) < 0)
			{
				LOG_ERROR2("lseek error errno = %d, %s\n", errno, strerror(errno));
			}
			
			return p;
		}
		
		// check again;
		return check_next_chunk(fd, p, osize + INC_MEM_SIZE, nsize);
	}
}

///
static char *read_one_line(int fd, int *size)
{
	char *p;
	int n;
	int index;

	*size = 0;
	
	p = (char *)malloc(INIT_MEM_SIZE * sizeof(char));
	assert(p != NULL);

	if ((n = read(fd, p, INIT_MEM_SIZE)) < 0)
	{
		LOG_ERROR1("read error: %s\n", strerror(errno));
		
		free(p);
		return NULL;
	}
	else if (n == 0)
	{
		// end of file
		free(p);
		return NULL;
	}
	else if (n < INIT_MEM_SIZE)
	{
		check_newline_index(fd, p, n, size);

		p[*size] = '\0';
		
		return p;
	}
	else
	{	// n == INIT_MEM_SIZE
		if ((index = get_newline_index(p, n)) >= 0)
		{
			*size = index + 1;

			// no room to save '\0'
			if (*size == INIT_MEM_SIZE)
			{
				p = realloc(p, *size + 1);
				assert(p != NULL);
			}
			
			p[*size] = '\0';
			
			// back the file ptr
			if (lseek(fd, -(n - *size), SEEK_CUR) < 0)
			{
				LOG_ERROR2("lseek error errno = %d, %s\n", errno, strerror(errno));
			}
			
			return p;
		}
		
		return check_next_chunk(fd, p, n, size);
	}
}

///
static line_t * fill_lines(int fd, int *nr_lines, line_t **line_tail, unsigned long *total_size)
{
	char *ptr;
	line_t *line, *prev = NULL;
	line_t *ret = NULL;
	int size;
	int nr = 0;

	*total_size = 0;

	for (;;)
	{
		ptr = read_one_line(fd, &size);
		if (!ptr)
		{
			break;
		}
		
		line = (line_t *)malloc(sizeof(line_t));
		assert(line != NULL);

		memset(line, 0, sizeof(line_t));

		line->line_ptr = ptr;
		line->line_size = size;
		line->line_next = NULL;
		INC_REFCNT(line);
		*total_size += size;
		
		if (!prev)
		{
			line->line_prev = NULL;
			ret = line;
		}
		else
		{
			prev->line_next = line;
			line->line_prev = prev;
		}

		prev = line;
		nr++;
	}

	*nr_lines = nr;

	*line_tail = prev;
	
	return ret;
}

//
void free_lines(line_t *header)
{
	line_t *node;
	line_t *tmp;
	char *ptr;

	
	for (node = header; node;)
	{
		if (node->line_buffer.refcnt == 0)
		{
			ptr = node->line_ptr;
			free(ptr);
		}
		else
		{
			DEC_REFCNT(node);
		}

		tmp = node;
		node = node->line_next;

		free(tmp);	 
	}
}


///
void print_content(int row, char *p)
{
	printf("%d\t%s", row,  p);
}


line_t * duplicate_lines(line_t *head, line_t **ret_tail)
{
	line_t *ret_head = NULL;
	line_t *tmp;
	line_t *prev = NULL;
	
	for (; head; head = head->line_next)
	{
		tmp = malloc(sizeof(line_t));
		assert(tmp != NULL);

		*tmp = *head;
		INC_REFCNT(tmp);

		tmp->line_prev = prev;
		tmp->line_next = NULL;

		if (prev)
		{
			prev->line_next = tmp;
		}
		else
		{
			ret_head = tmp;
		}

		prev = tmp;
	}

	*ret_tail = prev;

	return ret_head;
}


///
file_buffer_t *cmd_edit_file(const char *filename)
{
	int fd;
	line_t *line_head;
	line_t *line_tail;
	int total_lines;
	file_buffer_t *fb;

	fb = malloc(sizeof(file_buffer_t));
	assert(fb != NULL);

	memset(fb, 0, sizeof(file_buffer_t));


	strncpy(fb->filename, filename, MAX_FILENAME - 1);
	
	fd = open(filename, O_RDONLY, 0600);
	if (fd < 0)
	{
		LOG_ERROR1("open file %s failed!\n", filename);
		free(fb);
		return NULL;
	}

	line_head = fill_lines(fd, &total_lines, &line_tail, &(fb->filesize));

	printf("nr_lines = %d\n", total_lines);

	iterate_line_list(line_head, print_content);
	
	close(fd);	

	// fill file_buffer struct

	fb->current_address = total_lines;
	fb->end_address = total_lines;

	fb->head = line_head;
	fb->tail = line_tail;
	fb->dirty = FALSE;
	fb->error_code = ERR_OK;

	return fb;	
}

///
static void free_file_buffer(file_buffer_t *filep)
{
	if (!filep)
	{
		return;
	}

	free_lines(filep->head);
	free(filep);
}

///
static void iterate_line_list(const line_t *header, fn_t f)
{
	const line_t *tmp;
	int i;
	
	for (tmp = header, i = 1; tmp; tmp = tmp->line_next, i++)
	{
		f(i, tmp->line_ptr);
	}
}


int get_line_index(line_t *head, line_t *node)
{
	int index;
	line_t *p;

	if (!head)	// empty file
	{
		assert(node == NULL);
		return 0;
	}

	for (p = head, index = 0; p != node; p = p->line_next)
	{
		index++;
	}

	return index + 1;
}


line_t *get_line(line_t *head, int index)
{
	int i;
	line_t *tmp;
	line_t *prev;
	
	for (i = 0, tmp = head, prev = head; i < index; i++,  prev = tmp, tmp = tmp->line_next)
	{
		if (!tmp)
		{
			break;
		}
	}

	return  (i == index)? prev : NULL;
}

char *get_integer(char *buf, int *val)
{
	char *ptr = buf;
	int tmp;

	while (*ptr && isdigit(*ptr))
	{
		ptr++;
	}

	tmp = atoi(buf);
	*val = tmp;

	return ptr;
}



char *skip_blank(char *buf)
{
	char *ptr = buf;
	
	while (ptr && isblank(*ptr))
	{
		ptr++;
	}

	return ptr;
}


int get_special_address(file_buffer_t *filep, int ch)
{
	if (ch == '.')
		return filep->current_address;
	else if (ch == '$')
		return filep->end_address;

	return 0;
}

#define IS_SPECIAL_ADDR(x)	((x) == '.' || (x) == '$')


///
char *extract_address(file_buffer_t *filep, char *buf, int *pval, BOOLEAN *ok)
{
	char *p;
	int val = 0;
	int val2 = 0;
	int state = STATE_INITIAL;		/* 0	=>  initial state */
						/* 1	=>  got number */
						/* 2	=> see operator */	
	*ok = TRUE;
	*pval = 0;
	p = buf;
	while (*p)
	{
		switch (*p)
		{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '.':
			case '$':
			{	
				if (state == STATE_INITIAL || \
					state == STATE_GOT_ADD || \
					state == STATE_GOT_SUB)
				{
					if (!IS_SPECIAL_ADDR(*p))
					{
						if (state == STATE_INITIAL)
							p = get_integer(p, &val);	
						else
							p = get_integer(p, &val2);
					}
					else
					{
						if (state == STATE_INITIAL)
						{
							val = get_special_address(filep, *p);
						}
						else
						{
							val2 = get_special_address(filep, *p);
						}
						
						p++;
					}

					 if (state == STATE_GOT_ADD)		
						val += val2;
					else if (state == STATE_GOT_SUB)
						val -= val2;

					*pval = val;
				}
				else 
				{
					*ok = FALSE;

					return p;
				}

				state = STATE_GOT_NUMBER;
				
			}
			break;

			case '+':
			{
				if (state != STATE_GOT_NUMBER)
				{
					*ok = FALSE;
					return p;
				}

				state = STATE_GOT_ADD;
				p++;
				break;
			}

			case '-':	
			{
				if (state != STATE_GOT_NUMBER)
				{
					*pval = -1;
					return p;
				}
				
				state = STATE_GOT_SUB;
				p++;
				break;
			}

			case ' ':		// skip blank
			case '\t':
				p++;
				break;
			
			default:
			{
				if (state != STATE_GOT_NUMBER)
				{
					*ok = FALSE;
				}
				
				return p;
			}
		}
	}

	return p;
}


///
static char *get_file_name(char *ptr)
{
	static char tmp_name[20] = "/tmp/tmp.";
	char *p;

	ptr = skip_blank(ptr);

	p = ptr;
	while (p && *p && (*p != '\n'))
	{
		p++;
	}

	if (p != ptr)
	{
		*p = '\0';
		return (char *)ptr;
	}

	
	sprintf(tmp_name + 9, "%d", getpid());

	return tmp_name;
}

///
static void show_error_msg(file_buffer_t *filep)
{
	assert(filep->error_code < ERR_NUM);	
	if (filep->show_error)
	{
		printf("%s\n", error_msg[filep->error_code]);
	}
	else
	{
		printf("?\n");
	}

}

///
static BOOLEAN check_cmd_char(char ch)
{
	const char *p = &cmd_chars[0];

	while (*p)
	{
		if (*p == ch)
		{
			return TRUE;
		}
		
		p++;
	}

	return FALSE;
}


static void do_loop(file_buffer_t *filep)
{
	char buffer[BUFF_SIZE];
	char *p;
	int nr_start;
	int nr_end;
	BOOLEAN ok;

	filep->error_code = ERR_OK;
	
	while (1)
	{
		if (filep->show_prompt)
		{
			printf("%s ", filep->prompt_string);
		}
	
		if (!(p = fgets(buffer, BUFF_SIZE - 1, stdin)))
		{
			break;
		}

		printf("current_address = %d\n", filep->current_address);
		printf("end_address = %d\n", filep->end_address);

		fflush(stdout);
		
		assert((filep->tail == get_line(filep->head, filep->end_address)));
		assert((filep->head == get_line(filep->head, 1)));

		nr_start = 0;
		nr_end = 0;

		p = extract_address(filep, p, &nr_start, &ok);
		if (ok)
		{
			printf("nr_start = %d\n", nr_start);

			if (nr_start > filep->end_address)
			{
				// address is invalid
				filep->error_code = ERR_ADDRESS_INVALID;
				show_error_msg(filep);
				continue;
			}

			p = skip_blank(p);
			if (*p == ',')
			{
				p++;
				
				p = extract_address(filep, p, &nr_end, &ok);
				if (!ok || nr_end > filep->end_address)
				{
					printf("end address is invalid!\n");
					filep->error_code = ERR_ADDRESS_INVALID;
					show_error_msg(filep);
					continue;
				}

				printf("nr_end = %d\n", nr_end);

				if (nr_start > nr_end)
				{
					filep->error_code = ERR_ADDRESS_INVALID;
					show_error_msg(filep);
					continue;
				}
			}
		}
		else
		{
			printf("begin address is invalid!\n");

			if (*p == ',' || *p == '%')
			{
				nr_start = 1;
				nr_end  = filep->end_address;
				p++;
			}
			else if (*p == ';')
			{
				nr_start = filep->current_address;
				nr_end = filep->end_address;
				p++;
			}

			if (nr_start > nr_end)
			{
				filep->error_code = ERR_ADDRESS_INVALID;
				show_error_msg(filep);
				continue;
			}
			
			if (check_cmd_char(*p))
			{
				filep->error_code = ERR_OK;	
			}
			else
			{
				filep->error_code = ERR_ADDRESS_INVALID;
				show_error_msg(filep);
				continue;
			}
		}


		printf("p = %s\n", p);
#if 1
		switch (*p)
		{
			case 'a':
			{
				if (!nr_start && !nr_end)	// no address
				{
					nr_end = filep->current_address;
				}
				else if (nr_start && !nr_end)	// only one address
				{
					nr_end = nr_start;
				}

				cmd_append(filep, nr_start, nr_end);
				filep->error_code = ERR_OK;
				break;
			}

			case 'd':
			{
				if (!nr_start && !nr_end)	// no address
				{
					nr_end = filep->current_address;
				}
				else if (nr_start && !nr_end)	// only one address
				{
					nr_end = nr_start;
				}

				cmd_delete(filep, nr_start, nr_end);
				break;
			}

			case 'e':
			{
				const char *name;

				name = get_file_name(p + 1);
				
				free_file_buffer(filep);

				filep = cmd_edit_file(name);
				if (!filep)
				{
					return;
				}
				
				break;

			}

			case 'f':
			{
				const char *name;
				
				name = get_file_name(p + 1);
				strncpy(filep->filename, name, MAX_FILENAME);
				filep->error_code = ERR_OK;
				
				break;
			}

			case 'h':
			{
				assert(filep->error_code < ERR_NUM);

				printf("%s\n", error_msg[filep->error_code]);				
				break;
			}

			case 'H':
			{
				filep->show_error = !(filep->show_error);
				break;
			}

			case 'i':
			{
				if (!nr_start && !nr_end)	// no address
				{
					nr_end = filep->current_address;
				}
				else if (nr_start && !nr_end)	// only one address
				{
					nr_end = nr_start;
				}
				
				cmd_insert(filep, nr_start, nr_end);

				break;
				
			}

			case 'n':
			case 'p':
			{
				if (!nr_start && !nr_end)
				{
					nr_start = filep->current_address;
				}

				cmd_print(filep, nr_start, nr_end, *p == 'n');
				break;
			}

			case 'P':
			{
			
				filep->show_prompt = !filep->show_prompt;

				if (filep->show_prompt && filep->prompt_string[0] == '\0')
				{
					strcpy(filep->prompt_string, "*");
				}
				
				break;
			}

			case 'u':
			{
				cmd_undo(filep);
				
				break;
			}

			case 'w':
			{
				
				cmd_write(filep, nr_start, nr_end);
				
				break;
			}

			case 'q':
			{
				if (!nr_start && !nr_end)
				{
					nr_start = filep->current_address;
				}

				if (cmd_quit(filep, nr_start, nr_end))
				{
					return;	
				}
				else
				{
					filep->error_code = ERR_FILE_MODIFIED;
					show_error_msg(filep);
				}
				
				break;
			}

			case 'Q':
			{
				// exit uncondionaly
				return;
			}
			
			default:
				filep->error_code = ERR_UNKOWN_CMD;
				show_error_msg(filep);
				break;
		}
#endif	
		
	}

}

static void usage(char *name)
{
	char *p;
	
	
	if (!(p = strrchr(name, '/')))
	{
		p = name;
	}
	else
	{
		p++;
	}
	
	printf("%s [-p string] [filename]\n", p);

	exit(1);
}

#if 0
int main(void)
{
	char *p;

	p = malloc(128);

	printf("p = 0x%x\n", (unsigned int)p);
	memset(p, 0, 128);

	free(p);
	return 1;
}

#else
int main(int argc, char *argv[])
{
	file_buffer_t *filep;
	char prompt[MAX_PROMPT_SIZE];
	BOOLEAN show_prompt = FALSE;
	char *fname;
	int c;
	
	
	while ((c = getopt(argc, argv, "p:")) != -1)
	{
		switch (c)
		{
			case 'p':
				strncpy(prompt, optarg, MAX_PROMPT_SIZE - 1);
				show_prompt = TRUE;
				break;

			default:
				usage(argv[0]);
				break;
		}
	}

	printf("optind = %d\n", optind);
	
	fname = get_file_name(argv[optind]);	
	if ((filep = cmd_edit_file(fname)))
	{
		if (show_prompt)
		{
			filep->show_prompt = TRUE;
			strncpy(filep->prompt_string, prompt, MAX_PROMPT_SIZE - 1);
		}
		
		do_loop(filep);
		free_file_buffer(filep);
	}
	
	return 0;
}
#endif
