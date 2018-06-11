#include <cnix/types.h>
#include <cnix/kernel.h>
#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/driver.h>
#include <cnix/mm.h>
#include <cnix/net.h>
#include <cnix/errno.h>

typedef struct _ip_frag_node
{
	list_t list;		// next ip fragment node
	unsigned long time;	// The ticks
	skbuff_t *skb;		// ip fragment
	skbuff_t *own;		// skbuff which is used
} ip_frag_node_t;

#define IP_DEBUG

#define NO_FRAG		0
#define FIRST_FRAG	1
#define IN_FRAGS	2
#define LAST_FRAG	3
#define IP_FRAG_TIMEOUT		(30 * HZ)
#define IP_FRAG_RES		(HZ)

#define OFFSET(ip)	((ip->frag_off & FRAG_MASK) << 3)

static skbuff_t *cons_ip_dgram(skbuff_t *skb);
static skbuff_t *trim_ip_header(skbuff_t *skb);
static skbuff_t *insert_packet(skbuff_t *head, skbuff_t *skb);
static int get_frag_type(iphdr_t *ip);
static BOOLEAN is_packet_frag(iphdr_t *ip);
static BOOLEAN is_first_frag(iphdr_t *ip);
static BOOLEAN is_last_frag(iphdr_t *ip);
static skbuff_t *ip_fragment(skbuff_t *head, int mtu);
static BOOLEAN have_all_packet(skbuff_t *head);
static void trim_packets_header(skbuff_t *head);
static void check_frag_queue(void *data);
static void add_to_frag_queue(list_t *head, ip_frag_node_t *n);
static ip_frag_node_t *init_frag_node(skbuff_t *skb);
static ip_frag_node_t * get_new_node(void);
static void del_from_frag_queue(list_t *head, skbuff_t *skb);
int get_queue_count(list_t *head);

static list_t ip_frags_queue = {&ip_frags_queue, &ip_frags_queue};
static synctimer_t ip_frags_timer;
static BOOLEAN ip_timer_active = FALSE;

int print_ip_len(void)
{
	printk("lnkhdr len = %d\n", sizeof(ether_lnk_t));

	return 0;
}

BOOLEAN ip_forward = FALSE;

void input_ip_packet(skbuff_t *skb)
{
	iphdr_t *ip, ipbak;
	int hdr_len;
	int frag_type;
	ip_addr_t ip_addr;

	if(skb->dgram_len < IP_NORM_HEAD_LEN)
	{
		skb_free_packet(skb);
		return;
	}

	SK_TO_IP_HDR(ip, skb);

	// check version
	if (ip->version != IPV4)
	{
		printk("ip version = %d\n", ip->version);
		skb_free_packet(skb);
		return;
	}

	// check the header's checksum
	hdr_len = ip->ihl << 2;
	if ((skb->dgram_len < hdr_len) || ipchecksum(skb, hdr_len))
	{
		printk("ipchecksum error\n");
		skb_free_packet(skb);
		return;
	}

	// from network byte order to host byte order
	ip->tot_len = ntohs(ip->tot_len);
	ip->id = ntohs(ip->id);
	ip->frag_off = ntohs(ip->frag_off);

	if(skb->dgram_len < ip->tot_len) // allow larger, but not shorter
	{
		skb_free_packet(skb);
		return;
	}

	// check ttl
	if (!ip->ttl)
	{
		printk("discard the ip packet!\n");
		skb_free_packet(skb);
		return;
	}

	skb->dlen -= skb->dgram_len - ip->tot_len;
	skb->dgram_len = ip->tot_len;

	get_netif_ip_addr(skb->netif, &ip_addr);
	if (!ip_eq((ip_addr_t *)&ip->daddr, &ip_addr))
	{
		if (ip_forward)
		{
			// forward the ip packet to next route
		}
		else
		{
			// send the icmp packet to the source host.
		}

		skb_free_packet(skb);
		return;
	}

	frag_type = get_frag_type(ip);
	if (frag_type == NO_FRAG || (skb = cons_ip_dgram(skb)))
	{
		SK_TO_IP_HDR(ip, skb);
		ipbak = *ip;

		input_raw_packet(skb, &ipbak);

		// skb is the upper protocol data now
		skb = trim_ip_header(skb);
	
		switch (ip->protocol)
		{
			case IPPROTO_ICMP:
				input_icmp_packet(skb, &ipbak);
				return;

			case IPPROTO_TCP:
				input_tcp_packet(skb, &ipbak);
				return;

			case IPPROTO_UDP:
				input_udp_packet(skb, &ipbak);
				return;

			default:
				break;
		}

		skb_free_packet(skb);
	}
}

static void start_ip_frag_timer(void)
{	
	if (!ip_timer_active)
	{
		ip_frags_timer.expire = IP_FRAG_RES;		// 1s Check
		ip_frags_timer.data = &ip_frags_queue;
		ip_frags_timer.timerproc = check_frag_queue;
		synctimer_set(&ip_frags_timer);
		ip_timer_active = TRUE;
	}
}

// skbuff if we have got a whole ip datagram
// otherwise NULL
static skbuff_t *cons_ip_dgram(skbuff_t *skb)
{
	skbuff_t *head;
	skbuff_t *tmp;

//	printk("ip fragment count = %d!\n", get_queue_count(&ip_frags_queue));

	head = get_ip_hash_entry(skb);
	if (!head)
	{
		ip_frag_node_t *node;
		unsigned long flags;

		lockb_all(flags);
		
		node = init_frag_node(skb);		
		add_to_frag_queue(&ip_frags_queue, node);
		start_ip_frag_timer();
		
		add_ip_hash_entry(skb);

		unlockb_all(flags);

		return NULL;
	}

	// XXX: if insert failed, then discard all ip fragment
	tmp = insert_packet(head, skb);
	if (tmp != head)
	{
		unsigned long flags;

		lockb_all(flags);

		// update the hash table
		list_del1(&head->pkt_list);
		add_ip_hash_entry(tmp);

		unlockb_all(flags);
	}

#if 0
	{
		list_t *pos;
		list_t *node;
		skbuff_t *t;
		iphdr_t *ip;
		
		foreach(node, pos, &tmp->list)
			t = list_entry(node, list, skbuff_t);
			
			SK_TO_IP_HDR(ip, t);
			printk("offset = %d, pkt = %d\n",
				OFFSET(ip), t->dlen - t->data_ptr);
		endeach(&tmp->list);
	}
#endif

	if (have_all_packet(tmp))
	{
		// remove from ip frag queue
		del_from_frag_queue(&ip_frags_queue, tmp);
		
		list_del1(&tmp->pkt_list);	// remove from hash table
		trim_packets_header(tmp);

		return tmp;
	}
	else
	{
		return NULL;
	}
}

// remove the ip header, add the packet's offset to the skbuff dgram_len
static skbuff_t *trim_ip_header(skbuff_t *skb)
{
	iphdr_t *ip;
	int hdr_len;

	SK_TO_IP_HDR(ip, skb);
	hdr_len = ip->ihl << 2;

	skb->flag &= ~(SK_PK_HEAD | SK_PK_HEAD_DATA);
	skb->flag |= SK_PK_DATA;
	skb->data_ptr += hdr_len;
	skb->dgram_len -= hdr_len;

	return skb;
}

static void trim_packets_header(skbuff_t *head)
{
	iphdr_t *ip;
	list_t *pos;
	skbuff_t *skb;
	int size = 0;

	list_foreach_quick(pos, &head->list)
	{
		skb = list_entry(pos, list, skbuff_t);
		SK_TO_IP_HDR(ip, skb);

		// adjust the valid ip packet length
		skb->dlen = skb->data_ptr + ip->tot_len;
		skb->data_ptr += (ip->ihl << 2);
		skb->dgram_len = ip->tot_len - (ip->ihl << 2);
		size += skb->dlen - skb->data_ptr;
	}

	SK_TO_IP_HDR(ip, head);

	size += ip->tot_len;

	// adjust the valid dlen
	head->dlen = head->data_ptr + ip->tot_len;
	head->dgram_len = size;
	ip->tot_len = size;	// update the tot_len, checksum is invalid.
}

static BOOLEAN have_all_packet(skbuff_t *head)
{
	iphdr_t *ip;
	int index;
	int data_len;
	list_t *pos;
	list_t *tmp;
	skbuff_t *skb;
	skbuff_t *skb1;

	SK_TO_IP_HDR(ip, head);

	if (OFFSET(ip))
	{
		return FALSE;
	}

	index = 0;
	
	foreach(tmp, pos, &head->list)
		skb = list_entry(tmp, list, skbuff_t);

		SK_TO_IP_HDR(ip, skb);

		data_len = ip->tot_len - (ip->ihl << 2);

		index += data_len;
		if (pos != &head->list)
		{
			skb1= list_entry(pos, list, skbuff_t);

			SK_TO_IP_HDR(ip, skb1);

			if (index != OFFSET(ip))
			{
				return FALSE;
			}
		}
	endeach(&head->list);

	return (!(ip->frag_off & IP_MORE));
}

static BOOLEAN check_continous(skbuff_t *skb, skbuff_t *skb_next)
{
	iphdr_t *ip;
	iphdr_t *ip_next;
	int offset;
	int len;
	
	SK_TO_IP_HDR(ip, skb);
	SK_TO_IP_HDR(ip_next, skb_next);

	offset = OFFSET(ip);
	len = ip->tot_len - (ip->ihl << 2);

//	printk("offset1 = %d, len = %d, offset2 = %d\n", offset, len, OFFSET(ip_next));

	return (offset + len <= OFFSET(ip_next));	
}

static skbuff_t *insert_packet(skbuff_t *head, skbuff_t *skb)
{
	iphdr_t *ip;
	int offset ;	// skb
		
	skbuff_t *t1;	// tmp node
	iphdr_t *ip1;
	
	skbuff_t *t2;	// pos node
	iphdr_t *ip2;
	
	list_t *tmp;
	list_t *pos;
		
	SK_TO_IP_HDR(ip, skb);
	offset = OFFSET(ip);
	
	foreach(tmp, pos, &head->list)
		t1 = list_entry(tmp, list, skbuff_t);

		SK_TO_IP_HDR(ip1, t1);

		if (offset == OFFSET(ip1))	// duplicate packet.
		{
#if 0
			printk("packet duplicate @%d\n", offset);
#endif
			// replace old packet with new packet.
			pos = tmp;
			list_del1(tmp);
			skb_free(t1);
			tmp = pos;
			break;
		}

		if (tmp == &head->list)
		{
			// the first packet
			if (offset < OFFSET(ip1))
			{
				if (check_continous(skb, head))
				{
					list_init(&skb->list);
					list_add_tail(&skb->list, &head->list);
					
					head = skb;
				}
				else
				{
					skb_free(skb);
				}

				return head;				
			}
		}
		
		if (pos != &head->list)
		{
			t2 = list_entry(pos, list, skbuff_t);
			SK_TO_IP_HDR(ip2, t2);

			if (offset > OFFSET(ip1) && offset < OFFSET(ip2))
			{
				break;
			}
		}
		else
		{
			// the last packet
			if (offset > OFFSET(ip1))
			{
				break;
			}
		}
	endeach(&head->list);

	if (check_continous(t1, skb))
	{
		list_insert(tmp, &skb->list);
	}
	else
	{
		skb_free(skb);
	}
	
	return head;
}

static int get_frag_type(iphdr_t *ip)
{
	if (!is_packet_frag(ip))
	{
		return NO_FRAG;
	}

	if (is_first_frag(ip))
	{
		return FIRST_FRAG;
	}

	if (is_last_frag(ip))
	{
		return LAST_FRAG;
	}

	return IN_FRAGS;
}

// check wether the packet is fragmented or not
static BOOLEAN is_packet_frag(iphdr_t *ip)
{
	return (ip->frag_off & IP_MORE)  || OFFSET(ip) != 0;
}

// check wether the packet is the first fragment
static BOOLEAN is_first_frag(iphdr_t *ip)
{
	return ((ip->frag_off & IP_MORE) && (OFFSET(ip) == 0));
}

// check wether the packet is the last fragment
static BOOLEAN is_last_frag(iphdr_t *ip)
{
	return  (!(ip->frag_off & IP_MORE)) && (OFFSET(ip) != 0);
}

static void split_skb(
	iphdr_t * ip, skbuff_t * skb, int offset, list_t * head,
	netif_t * netif, int mtu
	)
{
	iphdr_t *ip1;
	unsigned char *p, *p1;
	int hdrlen, maxlen, data_index;

	if(skb->flag & SK_PK_DATA){
		hdrlen = IP_NORM_HEAD_LEN;
		data_index = skb->data_ptr;
	}else if(skb->flag & SK_PK_HEAD_DATA){
		hdrlen = ip->ihl << 2;
		data_index = skb->data_ptr + (ip->ihl << 2);
	}else if(skb->flag & SK_PK_HEAD){
		if(offset)
			DIE("BUG: cannot happen\n");

		ip->frag_off = htons((offset >> 3) | IP_MORE);
		ip->tot_len = htons(skb->dlen - skb->data_ptr);
		ip->check = 0;
		ip->check = ipchecksum(skb, ip->ihl << 2);

		skb->dgram_len = skb->dlen - skb->data_ptr;

		list_add_head(head, &skb->pkt_list);
		return;
	}else{
		DIE("BUG: cannot happen\n");
		return;
	}

	maxlen = mtu - hdrlen;

	while((skb->dlen - data_index) > maxlen){ // need spliting
		int len, flag;
		skbuff_t * tmp;

		len = mtu - IP_NORM_HEAD_LEN;
		flag = 0;

		if(list_empty(head)){ // last one
			len -= 8;
			len += (skb->dlen - data_index) % 8;
		}else
			flag = IP_MORE;

		// copy less as posibble as we could
		if(skb->dlen - data_index - len < maxlen) 
			len = (skb->dlen - data_index) - maxlen;

		tmp = skb_alloc(len + IP_NORM_HEAD_LEN + ETHER_LNK_HEAD_LEN, 1);

		tmp->dgram_len = len + IP_NORM_HEAD_LEN;
		tmp->data_ptr = tmp->dlen - tmp->dgram_len;
		tmp->flag |= SK_PK_HEAD_DATA;
		tmp->netif = netif;

		SK_TO_IP_HDR(ip1, tmp);
		memcpy(ip1, ip, IP_NORM_HEAD_LEN);
	
		offset -= len;

		// update ip header
		ip1->frag_off = htons((offset >> 3) | flag);

		ip1->tot_len = htons(len + IP_NORM_HEAD_LEN);
		ip1->check = 0;
		ip1->check = ipchecksum(tmp, IP_NORM_HEAD_LEN);

		// copy the data.
		p1 = sk_get_buff_ptr(tmp);
		p1 += tmp->data_ptr;
		p1 += IP_NORM_HEAD_LEN;

		p = sk_get_buff_ptr(skb);
		p += skb->dlen - len;

		memcpy(p1, p, len);

		skb->dlen -= len;

		list_add_head(head, &tmp->pkt_list);
	}

	offset -= skb->dlen - data_index;

	if(skb->flag & SK_PK_DATA){
		if(skb->data_ptr >= IP_NORM_HEAD_LEN + ETHER_LNK_HEAD_LEN){ // XXX
			skb->data_ptr -= IP_NORM_HEAD_LEN;
			skb->flag &= ~SK_PK_DATA;
			skb->flag |= SK_PK_HEAD_DATA;
			skb->netif = netif;
		}else{
			skbuff_t * tmp;

			tmp = skb_alloc(IP_NORM_HEAD_LEN + ETHER_LNK_HEAD_LEN, 1);

			tmp->data_ptr = ETHER_LNK_HEAD_LEN;
			tmp->flag |= SK_PK_HEAD;

			// only data, so sub skb->data_ptr instead of data_index
			tmp->dgram_len
				= IP_NORM_HEAD_LEN + skb->dlen - skb->data_ptr;
			tmp->netif = netif;

			list_add_tail(&tmp->list, &skb->list);

			skb = tmp;

			// not used
			//data_index = skb->data_ptr + IP_NORM_HEAD_LEN;
		}

		SK_TO_IP_HDR(ip1, skb);
		memcpy(ip1, ip, IP_NORM_HEAD_LEN);
	}else{
		if(offset)
			DIE("BUG: cannot happen\n");

		SK_TO_IP_HDR(ip1, skb);
	}

	// update ip header
	if(list_empty(head)) // last one
		ip1->frag_off = htons((offset >> 3));
	else
		ip1->frag_off = htons((offset >> 3) | IP_MORE);

	// dgram_len will be computed in if-condition before, if SK_PK_HEAD 
	if(skb->flag & SK_PK_HEAD_DATA){ // maybe change from SK_PK_DATA
		skb->dgram_len = skb->dlen - data_index + hdrlen;
#if 0
		// this won't happen
		if(skb->dlen - skb->data_ptr == hdrlen){ // if only head
			skb->flag &= ~SK_PK_HEAD_DATA;
			skb->flag |= SK_PK_HEAD;
		}
#endif
	}

	ip1->tot_len = htons(skb->dgram_len);
	ip1->check = 0;
	ip1->check = ipchecksum(skb, hdrlen);

	list_add_head(head, &skb->pkt_list);
}

/*
 * only be called under process context
 */
static skbuff_t *ip_fragment(skbuff_t *head, int mtu)
{
	int offset;
	iphdr_t * ip;
	list_t newhead;
	skbuff_t *next, *prev, *ret;

	list_head_init(&newhead);

	SK_TO_IP_HDR(ip, head);

	offset = head->dgram_len - (ip->ihl << 2);
	next = list_entry(head->list.prev, list, skbuff_t);

	do{
		int pkt_len;	

		if(next->flag & (SK_PK_HEAD | SK_PK_HEAD_DATA)){
			prev = NULL;
			pkt_len = next->dlen - next->data_ptr - (ip->ihl << 2);
		}else{
			prev = list_entry(next->list.prev, list, skbuff_t);
			pkt_len = next->dlen - next->data_ptr;
		}

		list_del1(&next->list); // XXX

		split_skb(ip, next, offset, &newhead, head->netif, mtu);

		offset -= pkt_len;
	}while((next = prev));

	ret = list_entry(newhead.next, pkt_list, skbuff_t);

	list_del1(&newhead);

	return ret;
}

static unsigned short gen_uniq_id(void)
{
	static unsigned short pos = 0;

	pos++;
	return pos;
}

/*
 * only be called under process context
 */
int output_ip_packet(skbuff_t *skb, ip_addr_t *ip_addr, int proto)
{
	BOOLEAN ok;
	iphdr_t *ip;
	int space, mtu;
	skbuff_t *head, *t;
	list_t *tmp, *pos;
	netif_t *netif;
	ip_addr_t local_ipaddr, gw_ipaddr;

	skb->netif = find_outif(ip_addr, &gw_ipaddr);
	if(!skb->netif){
		skb_free_packet(skb);
		return -EHOSTUNREACH; // XXX: error should come from find_outif
	}

	netif = skb->netif;
	mtu = netif->netif_mtu;
	space = skb->data_ptr;

	// ip option can only be generated under SOCK_RAW
	if (space < IP_NORM_HEAD_LEN)
	{
		// won't fail
		head = skb_alloc(IP_NORM_HEAD_LEN + ETHER_LNK_HEAD_LEN, 1);

		// update the head 
		head->flag |= SK_PK_HEAD;
		head->dgram_len = skb->dgram_len + IP_NORM_HEAD_LEN;
		head->netif = netif;
	
		skb->flag &= ~(SK_PK_HEAD | SK_PK_HEAD_DATA);
		skb->flag |= SK_PK_DATA;
		
		list_add_tail(&head->list, &skb->list);
	}
	else
	{
		if(skb->flag & SK_PK_DATA)
			DIE("BUG: cannot happen\n");

		skb->flag &= ~(SK_PK_DATA | SK_PK_HEAD);
		skb->flag |= SK_PK_HEAD_DATA;
		skb->dgram_len += IP_NORM_HEAD_LEN;
		head = skb;
	}

	head->data_ptr -= IP_NORM_HEAD_LEN;
	SK_TO_IP_HDR(ip, head);
	
	ip->version = IPV4;
	ip->ihl = IP_NORM_HEAD_LEN >> 2;
	ip->tos = 0;
	ip->id = htons(gen_uniq_id());
	ip->ttl = 32;
	ip->protocol = proto;
	ip->frag_off = 0;

	memcpy(&ip->daddr, ip_addr, IP_ADDR_LEN);
	get_netif_ip_addr(netif, &local_ipaddr);
	memcpy(&ip->saddr, &local_ipaddr, IP_ADDR_LEN);

	ip->tot_len = head->dgram_len;
	if (ip->tot_len > mtu)
	{
		if(ntohs(ip->frag_off) & IP_DF){
			skb_free_packet(head);
			return -EINVAL; // XXX
		}

		head = ip_fragment(head, mtu);
		//print_packets(head);
	}
	else
	{
		ip->tot_len = htons(ip->tot_len);
		ip->check = 0;
		ip->check = ipchecksum(head, IP_NORM_HEAD_LEN);
	}

	ok = TRUE;

	foreach(tmp, pos, &head->pkt_list)
		t = list_entry(tmp, pkt_list, skbuff_t);

		if(ok)
			ok = (*netif->netif_output)(
				netif, t, ip_addr, &gw_ipaddr
				);
		else
			skb_free_packet(t);
	endeach(&head->pkt_list);

	if(!ok)
		return -ENETUNREACH; // gateway is down, cannot get mac addr

	return 0;
}

// execute at PCNET32 half bottom queue
static ip_frag_node_t * get_new_node(void)
{
	skbuff_t *node;
	ip_frag_node_t *ret;
	unsigned char *p;

	node = skb_alloc(sizeof(ip_frag_node_t), 0);
	p = sk_get_buff_ptr(node);

	ret = (ip_frag_node_t *)p;
	
	ret->own = node;

	return ret;
}

static ip_frag_node_t *init_frag_node(skbuff_t *skb)
{
	ip_frag_node_t *node;

	node = get_new_node();
	if (!node)
	{
		PANIC("no free node for ip fragment!\n");
	}
	
	node->skb = skb;
	node->time = nowticks;
	list_init(&node->list);

	return node;
}

static void del_from_frag_queue(list_t *head, skbuff_t *skb)
{
	list_t *tmp;
	list_t *pos;
	ip_frag_node_t *t1;
	iphdr_t *ip;
	iphdr_t *ip1;
	skbuff_t *skb_tmp;

	SK_TO_IP_HDR(ip, skb);

	//printk("%s\n", __FUNCTION__);
	
	list_foreach (tmp, pos, head)
	{
		t1 = list_entry(tmp, list, ip_frag_node_t);
		skb_tmp = t1->skb;
		
		SK_TO_IP_HDR(ip1, skb_tmp);

		if (ip1->saddr == ip->saddr && ip1->daddr == ip->daddr && \
			ip1->id == ip->id)
		{
			break;
		}
	}

	if (tmp == head)
	{
		PANIC("Bug: can no happend!\n");
	}
	else
	{
		unsigned long flags;
		
		list_del1(tmp);
		skb_free(t1->own);		// free the node of own

		lockb_all(flags);

		if (list_empty(&ip_frags_queue))
		{
			ip_timer_active = FALSE;
			sync_delete(&ip_frags_timer);	// stop timer
		}

		unlockb_all(flags);
	}
}

// insert the n into head in order by time
static void add_to_frag_queue(list_t *head, ip_frag_node_t *n)
{
#if 1
	list_t *tmp;
	list_t *pos;
	ip_frag_node_t *t1;
	ip_frag_node_t *t2;
#endif

#if 0
	list_add_tail(head, &n->list);
#else
	list_foreach (tmp, pos, head)
	{
		t1 = list_entry(tmp, list, ip_frag_node_t);

		if (tmp->prev == head)
		{
			if (t1->time > n->time)
			{
				list_insert(head, &n->list);
				return;
			}
		}

		if (pos != head)
		{
			t2 = list_entry(pos, list, ip_frag_node_t);
		}
		else 
		{
			break;
		}

		if (t1->time <= n->time && n->time >= t2->time)
		{
			break;
		}
	}

	list_insert(tmp, &n->list);
#endif	
}

// execute at timer bottom half queue
static void free_ip_frags(skbuff_t *skb)
{
	skbuff_t *head;

	head = get_ip_hash_entry(skb);
	if (!head)
	{
		PANIC("BUG: can not happen!\n");
	}

	list_del1(&head->pkt_list); //delete from hash table.
	skb_free_packet(head);
}

// execute at timer bottom half queue
static void check_frag_queue(void *data)
{
	list_t *tmp;
	list_t *pos;
	list_t *head = (list_t *)data;
	ip_frag_node_t *t1;
	unsigned long flags;
	unsigned long cur_ticks = nowticks;

#ifdef IP_DEBUG
	//printk("check frag queue!\n");
#endif	

	lockb_all(flags);

	ip_timer_active = FALSE;

	list_foreach (tmp, pos, head)
	{
		t1 = list_entry(tmp, list, ip_frag_node_t);

		if (t1->time + IP_FRAG_TIMEOUT <= cur_ticks)
		{
			// timeout
			free_ip_frags(t1->skb);

			//printk(" timeout remove node");
			list_del1(tmp);
			skb_free(t1->own);
		}
		else
		{
			break;
		}
	}

	if (!list_empty(head))
	{
		start_ip_frag_timer();
	}

	unlockb_all(flags);
}
