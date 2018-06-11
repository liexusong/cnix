#ifndef _LIST_H
#define _LIST_H

#include <cnix/types.h>

typedef struct list_struct list_t;

struct list_struct{
	list_t * prev;
	list_t * next;
};

static inline void list_head_init(list_t * list)
{
	list->prev = list->next = list;
}

static inline void list_init(list_t * list)
{
	list->prev = list->next = list;
}

static inline i32_t list_empty(list_t * list)
{
	return list->prev == list;
}

static inline void list_add_head(list_t * list, list_t * n)
{
	n->next = list->next;
	list->next->prev = n;
	list->next = n;
	n->prev = list;
}

static inline void list_add_tail(list_t * list, list_t * n)
{
	n->prev = list->prev;
	list->prev->next = n;
	n->next = list;
	list->prev = n;
}

static inline void list_del(list_t * e)
{
#if 0
	if((e->prev == NULL) || (e->next == NULL))
		DIE("list error");
#endif

	e->prev->next = e->next;
	e->next->prev = e->prev;
#if 0
	e->prev = e->next = e;
#endif

#if 0
	e->prev = e->next = NULL;
#endif
}

/*
 * does a special initializing operation for fs/buffer.c and fs/inode.c,
 * they don't have a flag to show their state like in list or not.
 */
static inline void list_del1(list_t * e)
{
	e->prev->next = e->next;
	e->next->prev = e->prev;
	e->prev = e->next = e;
}

/* insert n after t */
static inline void list_insert(list_t *t, list_t *n)
{
	n->prev = t;
	n->next = t->next;

	t->next->prev = n;
	t->next = n;
}

#if 0
static inline void list_del_head(list_t * list, list_t ** n)
{
	*n = list->next;
	list_del(*n);
}

static inline void list_del_tail(list_t * list, list_t ** n)
{
	*n = list->prev;
	list_del(*n);
}
#endif

#define list_entry(list, name, type) \
	((type *)((unsigned long)(list) - (unsigned long)(&((type *)0)->name)))

/*
 * have head node
 * mismodified last time. use tmp in loop
 */
#define list_foreach(tmp, pos, list) \
	(tmp) = (list)->next; \
	for((pos) = (tmp)->next; \
		((tmp) != (list)); \
		(tmp) = (pos), (pos) = (pos)->next)

/*
 * have head node, but more quickly
 */
#define list_foreach_quick(pos, head) \
	for (pos = (head)->next; pos != (head); \
        	pos = pos->next)

/* no head node */
#define foreach(tmp, pos, list) \
	pos = (list); \
	do{ \
		tmp = pos; \
		pos = pos->next;

#define endeach(list) }while(pos != (list))

#endif
