#ifndef _MM_H
#define _MM_H

#include <cnix/types.h>
#include <cnix/const.h>
#include <cnix/head.h>
#include <cnix/page.h>

extern int kp_dir_size;
extern unsigned long start_mem;
extern unsigned long end_mem;
extern int nfreepages;
extern int ncodepages, ndatapages, nreservedpages, nmemmap_and_bitmap;
extern mem_map_t *mem_map;

extern unsigned long get_one_page(void);
extern unsigned long get_free_pages(unsigned long, int);
extern void free_one_page(unsigned long);
extern void __free_pages(unsigned long, int);

extern void copy_kernel_pts(unsigned long, unsigned long);

struct mmap_struct;

extern BOOLEAN copy_mmap_pts(
	struct mmap_struct *, unsigned long, unsigned long
	);
extern void free_mmap_pts(
	struct mmap_struct *,
	unsigned long
);

extern int free_page_tables(unsigned long, int, int);

extern char * kmalloc(int size,int flags);
extern void kfree(void *p);

#endif
