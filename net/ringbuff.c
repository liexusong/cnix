#include <cnix/mm.h>
#include <cnix/kernel.h>
#include <cnix/string.h>
#include <cnix/ringbuff.h>

void init_ring(ring_buffer_t * ring, int size)
{
	ring->data = NULL;
	if(size > 0)
		ring->data = (unsigned char *)kmalloc(size, PageWait);

	ring->head = 0;
	ring->count = 0;
	ring->size = size;
}

int copy_to_ring(
	const unsigned char * srcbuf, ring_buffer_t * ring, int data_len
	)
{
	int len;

	if(ring->count >= ring->size)
		return 0;

	if(data_len > ring->size - ring->count)
		data_len = ring->size - ring->count;

	len = ring->size - ring->head;

	if(len >= data_len)
		memcpy(&ring->data[ring->head], srcbuf, data_len);
	else{
		memcpy(&ring->data[ring->head], srcbuf, len);
		memcpy(ring->data, &srcbuf[len], data_len - len);
	}

	ring->head += data_len;
	ring->head %= ring->size;
	ring->count += data_len;

	return data_len;
}

/*
 * if rexmt, off is 0
 * if not rexmt, off is xmtseq - xmtack - 1
 */
int copy_from_ring(
	unsigned char * destbuf, ring_buffer_t * ring, int data_len,
	int off, BOOLEAN del // delete data or not
	)
{
	int len, idx;

	if(ring->count <= 0)
		return 0;

	if(off >= ring->count)
		DIE("BUG: cannot happen\n");

	if(data_len > ring->count)
		data_len = ring->count;

	idx = ring->head - ring->count + off;
	if(idx < 0)
		idx += ring->size;

	len = ring->size - idx;

	if(len >= data_len)
		memcpy(destbuf, &ring->data[idx], data_len);
	else{
		memcpy(destbuf, &ring->data[idx], len);
		memcpy(&destbuf[len], ring->data, data_len - len);
	}

	if(del)
		ring->count -= data_len;

	return data_len;
}

/*
 * have data or not
 */
BOOLEAN ring_is_empty(ring_buffer_t * ring)
{
	if(ring->count > 0)
		return FALSE;

	return TRUE;
}

/*
 * have spare space or not
 */
BOOLEAN ring_is_avail(ring_buffer_t * ring)
{
	if(ring->count < ring->size)
		return TRUE;

	return FALSE;
}
