#ifndef _PAGE_H
#define _PAGE_H

#include <cnix/const.h>
#include <cnix/head.h>
#include <cnix/config.h>

#define PAGE_DIR_ENTNUM		1024
#define PAGE_DIR_ENTSIZ		(PAGE_SIZE * 1024)
#define KP_DIR_ENTNUM		(USER_ADDR / PAGE_DIR_ENTSIZ)
#define USER_DIR_START		KP_DIR_ENTNUM
#define USER_DIR_ENTNUM		(PAGE_DIR_ENTNUM - KP_DIR_ENTNUM)

#define page_dir_idx(addr)	((addr) / PAGE_DIR_ENTSIZ)
#define to_pt_offset(x)		(((x) & (PAGE_DIR_ENTSIZ - 1)) >> PAGE_SHIFT)

#define  PAGE_MASK		(~(PAGE_SIZE - 1))
#define  FRAME_PER_PAGE		1024
#define  PAGE_ALIGN(addr)	((((addr)) + PAGE_SIZE - 1) & PAGE_MASK)
#define  MAP_NR(addr)		(((addr)) >> PAGE_SHIFT)
#define  _pa(addr)		(addr)

#ifndef NULL
#define NULL (unsigned long)0
#endif

#define PageReserved 	0x01
#define PageDMA		0x02
#define PageWait	0x04

typedef struct page{
	struct page * prev;
	struct page * next;
	unsigned long flags;
	int count;
}mem_map_t;

#endif
