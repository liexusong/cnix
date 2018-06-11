#ifndef _RINGBUFF
#define _RINGBUFF

#include <cnix/types.h>

typedef struct ring_buffer{
	unsigned char * data;
	int head;	// write positon, read position(head - count)
	int count;	// data bytes in buffer
	int size;	// buffer size
}ring_buffer_t;

extern void init_ring(ring_buffer_t * rbuff, int size);
extern int copy_to_ring(
	const unsigned char * srcbuf, ring_buffer_t * ring, int data_len
	);
extern int copy_from_ring(
	unsigned char * destbuf, ring_buffer_t * ring, int data_len,
	int off, BOOLEAN del
	);
extern BOOLEAN ring_is_empty(ring_buffer_t * ring);
extern BOOLEAN ring_is_avail(ring_buffer_t * ring);

#endif
