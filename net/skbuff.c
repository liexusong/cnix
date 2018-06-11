#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/skbuff.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>
#include <cnix/syslog.h>

extern BOOLEAN pcnet32_debug;

static list_t  skbfreelist;
static struct wait_queue * skb_wait;
int skb_total_nr = 0;	// this is the correct total skbuffer number
int skb_free_nr = 0;

void skbuff_init(void)
{
	int i, j;
	unsigned long addr;
	unsigned long offset;
	skbuff_t * skb;
	list_t *tmp;

	skb_wait = NULL;
	list_init(&skbfreelist);	
	
	for (i = 0; i < MAX_IPBUFF_NUM; i += (PAGE_SIZE / sizeof(skbuff_t)))
	{
		addr = get_one_page();
		if(!addr)
		{
			DIE("BUG: cannot happen\n");
		}

		for(j = 0; j < (PAGE_SIZE / sizeof(skbuff_t)); j++)
		{
			skb = (skbuff_t *)(addr + j * sizeof(skbuff_t));
			memset(skb, 0, sizeof(skbuff_t));

			list_init(&skb->list);
			list_init(&skb->pkt_list);
			list_add_tail(&skbfreelist, &skb->pkt_list);
			skb_total_nr++;
			skb_free_nr++;
		}
	}

	// allocate the static buffer
	offset = 0;
	list_foreach_quick(tmp, &skbfreelist)
	{
		if (!offset)
		{
			addr = get_one_page();
			if(!addr)
				DIE("BUG: cannot happen\n");
		}

		skb = list_entry(tmp, pkt_list, skbuff_t);

		skb->data = (unsigned char *)(offset + addr);
		skb->dlen = 0;
		skb->data_ptr = SKBUFFSIZE;
		skb->ext_data = 0;

		offset += SKBUFFSIZE;
		if (offset >= PAGE_SIZE)
		{
			offset = 0;
		}
	}

	printk("total skbuff nr = %d, free skbuff nr = %d\n", skb_total_nr, skb_free_nr);	
}

skbuff_t * skb_alloc(int size, int wait)
{
	unsigned long flags;
	skbuff_t * skb;
	list_t *node;
	int tmp;

	lock(flags); // XXX
try_again:
	if (list_empty(&skbfreelist))
	{
		if (!wait)
		{
			unlock(flags);
			return NULL;
		}

		sleepon(&skb_wait);
		goto try_again;
	}

	node = skbfreelist.next;
	list_del1(node);
	tmp = --skb_free_nr;
	unlock(flags);
	
	skb = list_entry(node, pkt_list, skbuff_t);
	list_init(&skb->list);
	list_init(&skb->pkt_list);
	
	skb->netif = NULL;
	skb->dlen = size;
	skb->dgram_len = 0;
	skb->flag = 0;
	skb->type = 0;
	skb->ext_data = NULL;

	if (pcnet32_debug)
	{
		do_syslog(LOG_KERN, "skb_alloc size = %d, free nr = %d, total nr = %d", 
			size, 
			tmp,
			skb_total_nr);
	}
	
	if(size > SKBUFFSIZE)
	{
		int flags = 0;

		if(wait)
			flags |= PageWait;

		skb->ext_data = (unsigned char *)kmalloc(size, flags);
		if (!skb->ext_data)
		{
			if(wait)
				DIE("BUG: cannot happen\n");

			lock(flags);
			list_add_head(&skbfreelist, node);
			unlock(flags);

			return NULL;
		}

		skb->flag = SK_EXT_MEM;
	}

	skb->data_ptr = skb->dlen;
	
	return skb;
}

void skb_free_packet(skbuff_t *skb)
{
	list_t *node;
	list_t *tmp;
	skbuff_t *buf;

	// free the packet body
	list_foreach(tmp, node, &skb->list)
	{
		list_del(tmp);

		buf = list_entry(tmp, list, skbuff_t);
		skb_free(buf);
	}

	// free the packet head
	skb_free(skb);
}

void skb_free(skbuff_t * skb)
{
	unsigned long flags;
	int tmp;

	// init the link list pointer
	list_init(&skb->list);
	list_init(&skb->pkt_list);
	
	skb->netif = NULL;
	skb->flag = 0;
	
	if(skb->ext_data)
	{
		kfree(skb->ext_data);
		skb->ext_data = NULL;
	}

	skb->dlen = 0;

	lock(flags);
	list_add_head(&skbfreelist, &skb->pkt_list);
	tmp = ++skb_free_nr;
	unlock(flags);

	if (pcnet32_debug)
	{
		do_syslog(LOG_KERN, "skb_free free nr = %d, total nr = %d", 
			tmp,
			skb_total_nr);
	}
}

// copy other except SK_EXT_MEM
static void copy_flag(int flag, int *newflag)
{
	BOOLEAN ext_mem;

	ext_mem = *newflag & SK_EXT_MEM;

	*newflag = flag;
	*newflag &= ~SK_EXT_MEM;
	if (ext_mem)
	{
		*newflag |= SK_EXT_MEM;
	}
}

/* duplicate the skb buffer, and copy the old valid data to the new one 
 * At least delta bytes will be left  from dlen to the end of memory.
 */
static skbuff_t *dup_skb(skbuff_t *skb, int delta, BOOLEAN wait)
{
	skbuff_t *ret;
	int pkt_len;
	unsigned char *p;
	unsigned char *p1;

	ret = skb_alloc(skb->dlen  + delta, wait);
	if (!ret)
	{
		return ret;
	}

	pkt_len = skb->dlen - skb->data_ptr;

	// copy & calculate the flag & the pointer
	copy_flag(skb->flag, &ret->flag);
	ret->type = skb->type;
	ret->dlen = skb->dlen;
	ret->data_ptr = skb->data_ptr;
	ret->dgram_len = skb->dgram_len;

	// copy the data. src pointer
	p = sk_get_buff_ptr(skb);
	p += skb->data_ptr;

	// dst pointer
	p1 = sk_get_buff_ptr(ret);
	p1 += ret->data_ptr;
	memcpy(p1, p, pkt_len);

	return ret;
}

/*
 * TODO:
 *   if this happens frequently, we should add a member to record buffer length.
 *   so needn't to alloc skb always, because maybe there is enough spare space.
 */
skbuff_t * skb_move_up(skbuff_t * skb, int len)
{
	int pkt_len;
	unsigned char * data, * data1;
	skbuff_t * skb1, * next;

	// check if ok.
	if (len <= skb->dlen - skb->data_ptr)
	{
		return skb;
	}

	skb1 = dup_skb(skb, len, FALSE);
	if (!skb1)
	{
		return NULL;
	}
	
	if(list_empty(&skb->list))
		DIE("BUG: cannot happen\n");

	pkt_len = skb1->dlen - skb1->data_ptr;
	len -= pkt_len;

	// how to check the end of list?
	next = list_entry(skb->list.next, list, skbuff_t);
	while (len > 0)
	{
		int copyed;

		if (next == skb)
		{	
			// goto the end.?
			break;
		}
		
		pkt_len = next->dlen - next->data_ptr;
		if (pkt_len >= len)
		{
			copyed = len;
		}
		else
		{
			copyed = pkt_len;
		}

		// src pointer
		data1 = sk_get_buff_ptr(next);
		data1 += next->data_ptr;
		// dst pointer
		data = sk_get_buff_ptr(skb1);
		data += skb1->dlen;
		
		memcpy(data, data1, copyed);

		// update the pointer
		next->data_ptr += copyed;
		skb1->dlen += copyed;
		len -= copyed;
		if (copyed == pkt_len)
		{
			skbuff_t *tmp;
			// free the null skbuf

			// how to check the end of list?
			tmp = list_entry(next->list.next, list, skbuff_t);
			list_del1(&next->list);
			skb_free(next);

			next = tmp;
		}
	}

	if (len)
	{
		skb_free(skb1);

		// copy failed.
		return NULL;
	}

	// make skb1 become the head of skb list
	list_add_head(&skb->list, &skb1->list);
	list_del1(&skb->list);

	skb_free(skb);
	return skb1;
}
