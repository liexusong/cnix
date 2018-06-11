#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#ifndef CNIX
#include <fcntl.h>
#endif

#include "ed.h"


#define MAX_BUFF_SIZE 	128

static void empty_undo_list(file_buffer_t *filep)
{
	undo_node_t *node;

	node = filep->undo_node;

	if (!node)
		return;

	if (node->cmd == 'i')
	{
		free_lines(node->undo_line_begin);
	}
	else if (node->cmd == 'd')
	{
		memset(node, 0, sizeof(undo_node_t));
	}
	else
		assert(FALSE);
}


///
static line_t *append_one_line(file_buffer_t * filep, line_t *row, const char *buf)
{
	char *p;
	int len;
	line_t *line;

	len = strlen(buf);
	
	p = malloc(len + 1);
	assert(p != NULL);
	strcpy(p, buf);

	line = malloc(sizeof(line_t));
	assert(line != NULL);

	line->line_ptr = p;
	line->line_size = len;

	if (row == NULL)
	{
		assert(filep->end_address == 0);
		
		line->line_prev = NULL;
		line->line_next = NULL;

		filep->head = line;
		filep->tail = line;
	}
	else
	{
		// row is the head node		
		line->line_prev = row;
		line->line_next = row->line_next;

		if (row->line_next)
		{
			row->line_next->line_prev = line;
		}
		else
		{	// row is the tail node
			filep->tail = line;
		}
		
		row->line_next = line;		
	}

	filep->current_address++;
	filep->end_address++;
	filep->dirty = TRUE;

	return line;
}

/*
 * insert some lines after current line
 */
line_t *cmd_append(file_buffer_t * filep, int nr_begin, int nr_end)
{
	char buff[MAX_BUFF_SIZE];
	char *p;
	line_t *end;
	undo_node_t *undo;
	int count;
	line_t *tmp;
	
	UNREFERENCED_PARAM(nr_begin);

	filep->current_address = nr_end;
	end = get_line(filep->head, nr_end);

	tmp = end;
	count = 0;
	while ((p = fgets(buff, MAX_BUFF_SIZE - 1, stdin)))
	{
		
		if (p[0] == '.' && p[1] == '\n' )
		{
			// the end
			break;
		}

		end = append_one_line(filep, end, p);
		count++;
	}

	if (count != 0)
	{
		// has append some lines
		if (!filep->undo_node)
		{
			filep->undo_node = malloc(sizeof(undo_node_t));
			assert(filep->undo_node != NULL);
			memset(filep->undo_node, 0, sizeof(undo_node_t));			
		}
		else
		{
			empty_undo_list(filep);
		}

		undo = filep->undo_node;

		if (tmp)
		{
			undo->undo_line_begin = tmp->line_next;
		}
		else
		{
			undo->undo_line_begin = filep->head;
		}
		
		undo->undo_line_end = end;

		undo->undo_index_begin = nr_end + 1;
		undo->undo_line_count = count;

		undo->cmd = 'd';		// delete
	}

	return end;
}


static void print_line(const char *linep, BOOLEAN show_nr, int row_nr)
{
	if (show_nr)
	{
		printf("%d\t%s", row_nr, linep);
	}
	else
	{
		printf("%s", linep);
	}

}

///
line_t *cmd_print(file_buffer_t * filep, int nr_begin, int nr_end, BOOLEAN show_nr)
{
	line_t *begin;
	line_t *end;
	
	if (nr_begin == 0)
	{
		return NULL;
	}

	filep->current_address = nr_begin;
	begin = get_line(filep->head, nr_begin);
	if (nr_end == 0)
	{
		print_line(begin->line_ptr, show_nr, filep->current_address);
		return begin;
	}

	end = get_line(filep->head, nr_end);
	
	for (; begin != end; begin = begin->line_next)
	{
		print_line(begin->line_ptr, show_nr, filep->current_address);
		filep->current_address++;
	}

	print_line(begin->line_ptr, show_nr, filep->current_address);
	
	return end;
}


///
static line_t *delete_one_line(file_buffer_t * filep, line_t *line)
{
	line_t *ret;
	
	if (!line)
	{
		return NULL;
	}

	ret = line->line_next;

	if (line->line_next)
	{
		line->line_next->line_prev = line->line_prev;
	}
	else	// line is the last node
	{
		filep->current_address--;
		filep->tail = line->line_prev;
		ret = line->line_prev;
	}

	if (line->line_prev)
	{
		line->line_prev->line_next = line->line_next;
	}
	else
	{
		// line is the first node
		filep->head = line->line_next;
	}

	filep->end_address--;
	filep->dirty = TRUE;

	return ret;	
}

static undo_node_t * new_undo_node(void)
{
	undo_node_t *ret;

	ret = malloc(sizeof(undo_node_t));
	assert(ret != NULL);
	memset(ret, 0, sizeof(undo_node_t));

	return ret;
}

///
line_t *cmd_delete(file_buffer_t * filep, int nr_begin, int nr_end)
{
	line_t *tmp;
	line_t *begin;
	line_t *end;
	undo_node_t *undo;
	int count = 0;
	
	if (nr_begin == 0 && nr_end == 0)
	{
		return NULL;
	}

	if (!(undo = filep->undo_node))
	{
		undo = malloc(sizeof(undo_node_t));
		assert(undo != NULL);

		memset(undo, 0, sizeof(undo));
		 filep->undo_node = undo;
	}
	else
	{
		// free undo node's content
		empty_undo_list(filep);
	}

	undo->cmd = 'i';
	
	if (nr_begin && nr_end == 0)
	{
		filep->current_address = nr_begin;
		begin = get_line(filep->head, nr_begin);

		// add to undo node
		undo->undo_index_begin = nr_begin;
		undo->undo_line_count = 1;
		undo->undo_line_begin = begin;
		undo->undo_line_end = begin;
		
		return delete_one_line(filep, begin);
	}

	if (nr_begin == 0 && nr_end)
	{
		filep->current_address = nr_end;
		end = get_line(filep->head, nr_end);
		
		//
		undo->undo_index_begin = nr_end;
		undo->undo_line_count = 1;
		undo->undo_line_begin = end;
		undo->undo_line_end = end;
		
		return delete_one_line(filep, end);
	}

	filep->current_address = nr_begin;
	begin = get_line(filep->head, nr_begin);
	end = get_line(filep->head, nr_end);

	assert(begin != NULL);
	assert(end != NULL);
	
	undo->undo_index_begin = nr_begin;

	for (tmp = begin;;)
	{
		count++;
		if (tmp == end)
		{
			break;
		}

		tmp = tmp->line_next;
	}

	tmp = tmp->line_next;		// return value
	undo->undo_line_count = count;

	if (begin->line_prev)
	{
		begin->line_prev->line_next = end->line_next;
	}
	else
	{
		filep->head = end->line_next;
	}

	if (end->line_next)
	{
		end->line_next->line_prev = begin->line_prev;
	}
	else
	{
		filep->tail = begin->line_prev;
	}

	undo->undo_line_begin = begin;
	undo->undo_line_end = end;
	end->line_next = NULL;
	begin->line_prev = NULL;

	filep->end_address -= count;
	if (filep->current_address > filep->end_address)
	{
		filep->current_address = filep->end_address;
	}

	return tmp;
}


static line_t *insert_one_line(file_buffer_t * filep, line_t *row, const char *ptr)
{
	int len;
	char *p;
	line_t *line;

	// get string buffer
	len = strlen(ptr);
	p = malloc(len + 1);
	assert(p != NULL);
	strcpy(p, ptr);

	// get one line buffer
	line = malloc(sizeof(line_t));
	assert(line != NULL);
	memset(line, 0, sizeof(line_t));

	// init line 
	line->line_ptr = p;
	line->line_size = len;

	// mark it as dirty
	filep->dirty = TRUE;
	
	if (!row)
	{
		filep->head = line;
		filep->tail = line;

		assert(filep->end_address == 0);

		filep->current_address = 1;
		filep->end_address = 1;

		return line;
	}

	// current line is the first line
	if (!row->line_prev)
	{
		line->line_next = row;
		row->line_prev = line;

		filep->head = line;

		filep->current_address = 1;
		filep->end_address++;

		return line;
	}

	line->line_next = row;
	line->line_prev = row->line_prev;

	row->line_prev->line_next = line;
	row->line_prev = line;

	filep->end_address++;

	return line;
}

///
line_t *cmd_insert(file_buffer_t * filep, int nr_begin, int nr_end)
{
	char buff[MAX_BUFF_SIZE];
	char *p;
	int len;
	BOOLEAN first_line = TRUE;
	line_t *end;
	undo_node_t *undo;
	int count = 0;
	line_t *head = NULL; // avoid gcc warning
	
	UNREFERENCED_PARAM(nr_begin);

	filep->current_address = nr_end;
	end = get_line(filep->head, nr_end);
	while ((p = fgets(buff, MAX_BUFF_SIZE - 1, stdin)))
	{
		len = strlen(p);
		
		if (p[0] == '.' && p[1] == '\n' )
		{
			// the end
			break;
		}

		if (first_line)
		{
			end = insert_one_line(filep, end, p);
			first_line = FALSE;
			head = end;
		}
		else
		{
			end = append_one_line(filep, end, p);
		}

		count++;
	}

	if (count == 0)
	{
		return end;
	}

	if (!(undo = filep->undo_node))
	{
		undo = new_undo_node();
		filep->undo_node = undo;
	}
	else
	{
		empty_undo_list(filep);
	}

	undo->cmd = 'd';
	undo->undo_index_begin = nr_end;
	undo->undo_line_count = count;

	undo->undo_line_begin = head;
	undo->undo_line_end = end;

	return end;	
}

///
BOOLEAN cmd_quit(file_buffer_t *filep, int nr_begin, int nr_end)
{
	UNREFERENCED_PARAM(nr_begin);
	UNREFERENCED_PARAM(nr_end);

	return (!filep->dirty);
}

///
static int write_nlines(file_buffer_t *filep, line_t *begin, int n)
{
	int fd;
	int size = 0;

	if ((fd = open(filep->filename, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0)
	{
		LOG_ERROR1("open %s for writing failed!\n", filep->filename);
		return 0;
	}

	for (; begin &&  n > 0; begin = begin->line_next, n--)
	{
		if (write(fd, begin->line_ptr, begin->line_size) != begin->line_size)
		{
			LOG_ERROR1("write %s failed!\n", filep->filename);
			goto done;
		}

		size += begin->line_size;
	}
	
	filep->dirty = FALSE;

done:
	close(fd);

	return size;
}

static void append_undo_list(file_buffer_t *filep)
{
	line_t *line = filep->tail;
	undo_node_t *node = filep->undo_node;

	if (line)
	{
		line->line_next = node->undo_line_begin;
		node->undo_line_begin->line_prev = line;

		filep->tail = node->undo_line_end;
		node->undo_line_end->line_next = NULL;
	}
	else
	{
		assert(filep->head == NULL);
		filep->head = node->undo_line_begin;
		filep->tail = node->undo_line_end;
	}

	filep->end_address += node->undo_line_count;
	filep->current_address = filep->end_address;
}

static void insert_undo_list(file_buffer_t *filep)
{
	line_t *line;
	undo_node_t *node = filep->undo_node;
	
	line = get_line(filep->head, node->undo_index_begin);
	assert(line != NULL);
	filep->current_address = node->undo_index_begin;

	node->undo_line_end->line_next = line;
	node->undo_line_begin->line_prev = line->line_prev;

	if (line->line_prev)
	{
		line->line_prev->line_next = node->undo_line_begin;	
	}
	else
	{
		filep->head = node->undo_line_begin;
	}

	line->line_prev = node->undo_line_end;

	filep->current_address += node->undo_line_count;
	filep->end_address += node->undo_line_count;
}

///
line_t * cmd_undo(file_buffer_t *filep)
{
	line_t *line;
	line_t *tmp;
	undo_node_t *node;
	
	line = get_line(filep->head, filep->current_address);
	if (!filep->undo_node)
	{
		return line;
	}

	node = filep->undo_node;
	switch (node->cmd)
	{
		case 'd':
			line = node->undo_line_begin;
			filep->current_address = get_line_index(filep->head, line);

			for (;;)
			{	
				tmp = line;
				delete_one_line(filep, tmp);
				
				line = line->line_next;
				DEC_REFCNT(tmp);
				if (tmp->line_refcnt == 0)
				{
					// free one node.
					free(tmp->line_ptr);
				}
				free(tmp);

				if (tmp == node->undo_line_end)
				{
					break;
				}
			}
			
			break;

		case 'i':
			if (node->undo_index_begin > filep->end_address)
			{
				append_undo_list(filep);
			}
			else
			{
				insert_undo_list(filep);
			}
			
			break;

		default:
			assert(FALSE);
	}

	free(node);
	filep->undo_node = NULL;	
			
	return get_line(filep->head, filep->current_address);
}

///
void cmd_write(file_buffer_t *filep, int nr_begin, int nr_end)
{
	int size = 0;
	line_t *begin;
	line_t *end;
	
	if ((nr_begin == 0 && nr_end  == 0))
	{
		size = write_nlines(filep, filep->head, filep->end_address);
		goto done;
	}

	if (nr_begin == 0 && nr_end != 0)
	{
		end = get_line(filep->head, nr_end);
		size = write_nlines(filep, end, 1);
		return;
	}

	if (nr_begin != 0 && nr_end == 0)
	{
		begin = get_line(filep->head, nr_begin);
		size = write_nlines(filep, begin, 1);
		return;
	}

	begin = get_line(filep->head, nr_begin);
	
	size = write_nlines(filep, begin, nr_end - nr_begin + 1);
done:
	printf("%d\n", size);
}
