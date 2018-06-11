#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>
#include <cnix/net.h>

typedef struct ip_reply_node
{
	list_t list;
	skbuff_t *skb;
	ip_addr_t ip_addr;
	int proto;
} ip_reply_t;

static list_t ip_reply_list;
static list_t ip_reply_free_list;

#define MAX_REPLY_NR 16

static ip_reply_t reply_array[MAX_REPLY_NR];
static struct wait_queue *ip_reply_wait = NULL;
static BOOLEAN ip_reply_running = FALSE;

BOOLEAN add_reply_list(skbuff_t *skb, ip_addr_t *ip_addr, int proto)
{
	list_t *tmp;
	ip_reply_t *r;

	if (list_empty(&ip_reply_free_list) || !ip_reply_running)
	{
		return FALSE;
	}
	
	tmp = ip_reply_free_list.next;
	list_del1(tmp);

	r = list_entry(tmp, list, ip_reply_t);
	r->skb = skb;
	memcpy(&r->ip_addr, ip_addr, IP_ADDR_LEN);
	r->proto = proto;

	list_add_tail(&ip_reply_list, tmp);

	wakeup(&ip_reply_wait);

	return TRUE;
}

void ip_reply_daemon(void *data)
{
	int i;
	unsigned long flags;

	strcpy(current->myname, "ip packet daemon");
	
	list_init(&ip_reply_list);
	list_init(&ip_reply_free_list);

	for (i = 0; i < MAX_REPLY_NR; i++)
	{
		list_init(&reply_array[i].list);
		list_add_tail(&ip_reply_free_list, &reply_array[i].list);
	}

	ip_reply_running = TRUE;

	for (;;)
	{
		sleepon(&ip_reply_wait);

		lock(flags);

		while(!list_empty(&ip_reply_list))
		{
			list_t *tmp;
			ip_reply_t *r;
			skbuff_t * skb;
			ip_addr_t ip_addr;
			int proto;

			tmp = ip_reply_list.next;
			list_del1(tmp);	

			r = list_entry(tmp, list, ip_reply_t);
			skb = r->skb;
			ip_addr = r->ip_addr;
			proto = r->proto;

			list_add_tail(&ip_reply_free_list, tmp);

			unlock(flags);

			output_ip_packet(skb, &ip_addr, proto);

			lock(flags);	
		}

		unlock(flags);
	}
}
