#ifndef _BUFFER_H
#define _BUFFER_H

#include <cnix/types.h>
#include <cnix/wait.h>
#include <cnix/list.h>

#define NR_BUF_PAGES	4096
#define NR_BUF		(4 * NR_BUF_PAGES)
#define NR_HASH		(NR_BUF / 8)

#define BLOCKSIZ	0x400

#define NODEV		0x7fff

#define WRITE		0
#define READ		1

#define NOZONE		0
#define NOBLOCK		0

struct buf_head{
	int b_dev;        
	int b_blocknr;
	short int b_flags;
	int b_count;
	struct wait_queue * b_wait;
	unsigned char * b_data;
	list_t b_rel;
	list_t b_rel_free;
};

#define B_WRITE 0
#define B_READ  01
/* The buf is not in free list, it has been in use */
#define B_BUSY  02    
/* 
 * delay write, the buf is different from disk, so if release it, 
 * must be written to disk
 */ 
#define B_DIRTY		04  
/* disk io complete, the buf contains validated data */ 
#define B_DONE		010 
/* another proc want this buf, but it is busy , so the proc must sleep */
#define B_WANTED	020	
/* the disk i/o has some errors, data can not be retrieve */
#define B_ERROR		040	
/* asynchronize write or read, not wait the io complicaton */
#define B_ASY		0100	

extern void bsync(void);
extern void inval_dev(dev_t);
extern void inval_block(dev_t, block_t);

#endif
