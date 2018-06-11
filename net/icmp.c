#include <cnix/types.h>
#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/driver.h>
#include <cnix/mm.h>
#include <cnix/net.h>

#define USE_OLD_SKB 1
typedef struct _timestamp
{
	u32_t utc_send_time;
	u32_t utc_rcv_time;
	u32_t utc_xmt_time;
} timestamp_t;

static skbuff_t *cons_icmp_echo_packet(skbuff_t *skb, iphdr_t * ip);
static skbuff_t *dup_icmp_packet(skbuff_t *skb, int len);
static skbuff_t *cons_icmp_timestamp_packet(skbuff_t *skb, iphdr_t *ip);
static skbuff_t *cons_icmp_netmask_packet(skbuff_t *skb, iphdr_t *ip);

void input_icmp_packet(skbuff_t *skb, iphdr_t *ip)
{
	int len;
	icmphdr_t *icmp;
	skbuff_t *reply;
	
	if(skb->dgram_len < sizeof(icmphdr_t)){
		skb_free_packet(skb);
		return;
	}

	if(skb->dlen - skb->data_ptr < sizeof(icmphdr_t)){
		skbuff_t * skb1;

		printk("need testing, move icmp data up\n");

		if(!(skb1 = skb_move_up(skb, sizeof(icmphdr_t)))){
			skb_free_packet(skb);
			return;
		}

		skb = skb1;
	}

	SK_TO_ICMP_HDR(icmp, skb);
	len = skb->dgram_len;

	if (checksum(skb))
	{
		printk("icmp checksum error!\n");

		skb_free_packet(skb);
		return;
	}

	switch (icmp->type)
	{
		case ICMP_ECHO:
			if (icmp->code == 0)
			{
				reply = cons_icmp_echo_packet(skb, ip);
				if (!reply)
				{
					break;
				}

				if (!add_reply_list(reply,
					(ip_addr_t *)&ip->saddr, IPPROTO_ICMP))
				{
					skb_free_packet(reply);
				}
#if USE_OLD_SKB
				return;
#endif
			}
			else
			{
				printk("icmp echo, code %d\n", icmp->code);
			}
	
			break;
			
		case ICMP_TIMESTAMP:
			if (icmp->code == 0)
			{
				if (!(reply = cons_icmp_timestamp_packet(skb, ip)))
				{
					break;
				}
				
				if (!add_reply_list(reply,
					(ip_addr_t *)&ip->saddr, IPPROTO_ICMP))
				{
					skb_free_packet(reply);
				}

				break;
			}
			else 
			{
				printk("icmp timestamp error, code %d\n", icmp->code);
			}

			break;
			
		case ICMP_ADDRESS:
			if (icmp->code == 0)
			{
				if (!(reply = cons_icmp_netmask_packet(skb, ip)))
				{
					break;
				}
				
				if (!add_reply_list(reply,
					(ip_addr_t *)&ip->saddr, IPPROTO_ICMP))
				{
					skb_free_packet(reply);
				}

				break;
			}
			
			break;

		default:
			//printk("icmp_type = %d\n", icmp->type);
			break;
	}

	skb_free_packet(skb);
}

#if USE_OLD_SKB
static skbuff_t *cons_icmp_echo_packet(skbuff_t *skb, iphdr_t * ip)
{
	icmphdr_t *icmp;

	if(skb->dlen - skb->data_ptr < sizeof(icmphdr_t)){
		printk("get a strange icmp packet\n");
		return NULL;
	}

	SK_TO_ICMP_HDR(icmp, skb);

	icmp->type = ICMP_ECHOREPLY;
	icmp->checksum = 0;
	icmp->checksum = checksum(skb);

	skb->flag &= ~(SK_PK_HEAD | SK_PK_DATA);
	skb->flag |= SK_PK_HEAD_DATA;

	{

		skbuff_t * skb1;
		list_t * tmp, * pos;

		list_foreach(tmp, pos, &skb->list){
			skb1 = list_entry(tmp, list, skbuff_t);
			skb1->flag &= ~(SK_PK_HEAD | SK_PK_HEAD_DATA);
			skb1->flag |= SK_PK_DATA;
		}
	}

	return skb;
}
#else
static skbuff_t *cons_icmp_echo_packet(skbuff_t *skb, iphdr_t * ip)
{
	skbuff_t *r;
	skbuff_t *t;
	icmphdr_t *icmp;
	list_t *tmp;
	list_t *pos;
	unsigned char *p;
	unsigned char *q;

	r = skb_alloc(skb->dgram_len + IP_NORM_HEAD_LEN + ETHER_LNK_HEAD_LEN,
			0);
	if(!r)
	{
		printk("no free skbuff!\n");
		return NULL;
	}

	//printk("len = %d\n", skb->dgram_len);
	r->flag |= SK_PK_HEAD_DATA;
	r->data_ptr = r->dlen - skb->dgram_len;
	r->dgram_len = skb->dgram_len;

	q = sk_get_buff_ptr(r);
	q += r->data_ptr;

	foreach(tmp, pos, &skb->list)
		t = list_entry(tmp, list, skbuff_t);

		p = sk_get_buff_ptr(t);
		p += t->data_ptr;

		memcpy(q, p, t->dlen - t->data_ptr);

		q += t->dlen - t->data_ptr;
	endeach(&skb->list);

	SK_TO_ICMP_HDR(icmp, r);
	
	icmp->checksum = 0;
	icmp->type = ICMP_ECHOREPLY;
	icmp->code = 0;
	
	icmp->checksum = checksum(r);

	return r;
}
#endif

static skbuff_t *cons_icmp_netmask_packet(skbuff_t *skb, iphdr_t *ip)
{
	int len;
	skbuff_t *r;
	icmphdr_t *icmp;
	unsigned char *p;
	ip_addr_t mask;
		
	len = skb->dgram_len;
	
	if (!(r = dup_icmp_packet(skb, len)))
	{
		return NULL;
	}

	SK_TO_ICMP_HDR(icmp, r);
	icmp->type = ICMP_ADDRESSREPLY;
	icmp->checksum = 0;
	icmp->code = 0;
	
	p = sk_get_buff_ptr(r);
	p += r->data_ptr;
	p += sizeof(icmphdr_t);
	get_netif_netmask(skb->netif, &mask);
	*(u32_t *)p = iptoul(mask);
	
	printk("netmask = ");
	print_ip_addr(&mask);
	printk("\n");
	
	icmp->checksum = checksum(r);

//	print_packet(r, 1);
	
	return r;
}

static skbuff_t *cons_icmp_timestamp_packet(skbuff_t *skb, iphdr_t *ip)
{
	int len;
	skbuff_t *r;
	icmphdr_t *icmp;
	timestamp_t *ts;
	unsigned char *p;
		
	len = skb->dgram_len;
	
	if (!(r = dup_icmp_packet(skb, len)))
	{
		return NULL;
	}	

	SK_TO_ICMP_HDR(icmp, r);
	icmp->type = ICMP_TIMESTAMPREPLY;
	icmp->checksum = 0;
	icmp->code = 0;
	
	p = sk_get_buff_ptr(r);
	p += r->data_ptr;
	p += sizeof(icmphdr_t);
	ts = (timestamp_t *)p;
	// XXX:
	ts->utc_rcv_time = htonl(utc_time_diff());
	ts->utc_xmt_time = ts->utc_rcv_time;
	printk("utc_rcv_time = %d\n", ntohl(ts->utc_rcv_time));
	icmp->checksum = checksum(r);

//	print_packet(r, 1);
	
	return r;
}

// duplicate the skbuffer, copy len data from skb to new skbuff
static skbuff_t *dup_icmp_packet(skbuff_t *skb, int len)
{
	skbuff_t *r;
	skbuff_t *t;
	list_t *tmp;
	list_t *pos;
	unsigned char *p;
	unsigned char *q;

	r = skb_alloc(len + IP_NORM_HEAD_LEN + ETHER_LNK_HEAD_LEN, 0);
	if(!r)
	{
		printk("no free skbuff!\n");
		return NULL;
	}

	printk("len = %d\n", len);
	r->flag |= SK_PK_HEAD_DATA;
	r->data_ptr = r->dlen - len;
	r->dgram_len = len;

	q = sk_get_buff_ptr(r);
	q += r->data_ptr;

	foreach(tmp, pos, &skb->list)
	{
		int copyed;
		
		t = list_entry(tmp, list, skbuff_t);

		p = sk_get_buff_ptr(t);
		p += t->data_ptr;

		copyed = t->dlen - t->data_ptr;
		if (copyed > len)
		{
			copyed = len;
		}

		memcpy(q, p, copyed);

		q += copyed;
		len -= copyed;

		if (!len)
		{
			break;
		}
	}
	endeach(&skb->list);

	return r;
}

// skb is the ip packet.
static skbuff_t *cons_icmp_error_packet(skbuff_t *skb, int type, int code)
{
	skbuff_t *r;
	icmphdr_t *icmp;
	iphdr_t *ip;
	int len;

	SK_TO_IP_HDR(ip, skb);

	len = (ip->ihl << 2) + 8;

	if (!(r = dup_icmp_packet(skb, len)))
	{
		return NULL;
	}
	
	r->data_ptr -= sizeof(icmphdr_t);
	r->dgram_len += sizeof(icmphdr_t);

	SK_TO_ICMP_HDR(icmp, r);
	
	icmp->checksum = 0;
	icmp->type = type;
	icmp->code = code;	
	icmp->checksum = checksum(r);

	return r;
}

// skb is the old ip packet.
void send_icmp_error_packet(skbuff_t *skb, int type, int code)
{
	iphdr_t *ip;
	skbuff_t *r;
	ip_addr_t ip_addr;

	SK_TO_IP_HDR(ip, skb);

	ip_addr = ultoip(ip->saddr);

	if (!(r = cons_icmp_error_packet(skb, type, code)))
	{
		return;
	}

	if (!add_reply_list(r, &ip_addr, IPPROTO_ICMP))
	{
		skb_free(r);
	}
	
}

