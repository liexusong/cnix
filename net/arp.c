#include <asm/io.h>
#include <asm/system.h>
#include <cnix/types.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/driver.h>
#include <cnix/mm.h>
#include <cnix/net.h>

extern void switch_pcnet32_debug(void);
extern BOOLEAN pcnet32_debug;

typedef struct _arp_request
{
	list_t list;
	ether_netif_t *netif;
	ip_addr_t ip_addr;
	struct wait_queue *arp_wait;
} arp_req_t;

struct timeout_struct
{
	int tmout;
	struct wait_queue **wait;
};

static list_t arp_free_list;
static list_t arp_head;
static arp_entry_t arp_table[MAX_ARP_ENTRY];

static list_t arp_req_head;
static list_t arp_req_free_list;
static struct wait_queue *req_free_wait;
static arp_req_t arp_reqs[MAX_ARP_REQ];

static void cons_reply_arp_packet(skbuff_t * skb);
static void emit_req(arp_req_t *req);

static void start_arp_live_timer(void);

void init_arp_queue(void)
{
	int i;
	arp_entry_t *pentry;
	arp_req_t *preq;

	list_init(&arp_head);
	list_init(&arp_free_list);
	
	list_init(&arp_req_head);
	list_init(&arp_req_free_list);

	req_free_wait = NULL;

	for (i = 0; i < MAX_ARP_ENTRY; i++)
	{
		pentry = &arp_table[i];
		list_init(&pentry->list);
		list_add_tail(&arp_free_list, &pentry->list);
	}

	for (i = 0; i < MAX_ARP_REQ; i++)
	{
		preq =  &arp_reqs[i];
		list_init(&preq->list);
		list_add_tail(&arp_req_free_list, &preq->list);
	}

	start_arp_live_timer();
}

static synctimer_t arp_live_timer;

// timeout callback function
static void arp_live_timeout_proc(void * data)
{
	list_t *pos;
	list_t *tmp;
	arp_entry_t *pentry;

	list_foreach(tmp, pos, &arp_head)
	{
		pentry = list_entry(tmp, list, arp_entry_t);

		pentry->life_time--;
		if (pentry->life_time <= 0)
		{
			list_del1(tmp);
			list_add_tail(&arp_free_list, tmp);
		}
	}

	start_arp_live_timer();
}

static void start_arp_live_timer(void)
{
	arp_live_timer.expire = MS2HZ(60 * 1000);
	arp_live_timer.data = NULL;
	arp_live_timer.timerproc = arp_live_timeout_proc;

	synctimer_set(&arp_live_timer);
}

/*
 * caller should lock(flags)
 */
static arp_entry_t * find_arp_entry(ip_addr_t *ip_addr)
{
	arp_entry_t *tmp;
	list_t *pos;

	list_foreach_quick(pos, &arp_head)
	{
		tmp = list_entry(pos, list, arp_entry_t);
	
		if (ip_eq(&tmp->ip_addr, ip_addr))
		{
			return tmp;
		}
	}

	return NULL;
}

static BOOLEAN get_arp_entry(ip_addr_t *ip_addr, arp_entry_t *pentry)
{
	arp_entry_t *tmp;
	list_t *pos;
	unsigned long flags;

	lock(flags);

	list_foreach_quick(pos, &arp_head)
	{
		tmp = list_entry(pos, list, arp_entry_t);
	
		if (ip_eq(&tmp->ip_addr, ip_addr))
		{
			*pentry = *tmp;
			unlock(flags);

			return TRUE;
		}
	}

	unlock(flags);
	
	return FALSE;
}

static void add_arp_entry(mac_addr_t *mac_addr, ip_addr_t *ip_addr)
{
	list_t *tmp;
	arp_entry_t *pentry;
	unsigned long flags;

	lock(flags);

	if ((pentry = find_arp_entry(ip_addr))){
		tmp = &pentry->list;
		list_del1(tmp);
	}else if (list_empty(&arp_free_list)){
		// delete the first node.
		tmp = arp_head.next;
		list_del1(tmp);
	}else{
		tmp = arp_free_list.next;
		list_del1(tmp);
	}

	unlock(flags);
	
	pentry = list_entry(tmp, list, arp_entry_t);

	memcpy(&pentry->ip_addr, ip_addr, IP_ADDR_LEN);
	memcpy(&pentry->mac_addr, mac_addr, MAC_ADDR_LEN);
	pentry->life_time = INIT_ARP_TIME;

	lock(flags);
	list_add_tail(&arp_head, tmp);
	unlock(flags);
}

void print_arp_table(void)
{
	arp_entry_t *entry;
	list_t *pos;

	printk("arp table:\n");

	list_foreach_quick(pos, &arp_head)
	{
		entry = list_entry(pos, list, arp_entry_t);

		print_ip_addr(&entry->ip_addr);
		printk("\t");
		print_mac_addr(&entry->mac_addr);
		printk("\n");
	}

	switch_pcnet32_debug();
}

// construct the arp request package at interface pdev
static skbuff_t * cons_arp_req_packet(ether_netif_t * pdev,
					ip_addr_t *ip_req)
{
	int pk_size = sizeof(arp_packet_t);
	skbuff_t *skb;
	unsigned char *ptr;
	arp_packet_t *parp;
	mac_addr_t mac;
	ip_addr_t ip;
	
	// won't fail
	skb = skb_alloc(pk_size, 1);

	skb->dgram_len = pk_size;
	skb->data_ptr = skb->dlen - pk_size;
	skb->type = 0;
	skb->netif = (netif_t *)pdev;
	
	ptr = sk_get_buff_ptr(skb);
	ptr += skb->data_ptr;
	parp = (arp_packet_t *)ptr;

	get_ether_boardcast_addr(pdev, &mac);
	memcpy(&parp->mac_dst, &mac, MAC_ADDR_LEN);

	get_ether_mac_addr(pdev, &mac);
	memcpy(&parp->mac_src, &mac, MAC_ADDR_LEN);
	
	parp->packet_type = htons(ARP_TYPE);
	parp->h_type = htons(H_ETHER_ADDR_TYPE);
	parp->p_type = htons(P_IP_ADDR_TYPE);
	parp->h_len = MAC_ADDR_LEN;
	parp->p_len = IP_ADDR_LEN;
	parp->op_type = htons(ARP_REQ);
	
	memcpy(&parp->mac_sender, &mac, MAC_ADDR_LEN);
	get_netif_ip_addr((netif_t *)pdev, &ip);
	memcpy(&parp->ip_sender, &ip, IP_ADDR_LEN);
	memset(&parp->mac_sendback, 0, MAC_ADDR_LEN);
	memcpy(&parp->ip_sendback, ip_req, IP_ADDR_LEN);

	return skb;	
}

static void arp_timeout_callback(void *data)
{
	struct timeout_struct *t = (struct timeout_struct *)data;

	t->tmout = 1;
	wakeup(t->wait);
}

static void init_arp_timer(
	int ms,
	arp_req_t *req,
	struct timeout_struct *arp_tm_data,
	synctimer_t *arp_timer
	)
{
	// setup timer
	arp_tm_data->tmout = 0;
	arp_tm_data->wait = &req->arp_wait;
	
	arp_timer->expire = MS2HZ(ms);
	arp_timer->data = arp_tm_data;
	arp_timer->timerproc = arp_timeout_callback;

	synctimer_set(arp_timer);
}
 
/*
 * send the arp request, only be called under process context
 */
static void emit_req(arp_req_t *req)
{
	skbuff_t *skb;
	ether_netif_t *netif = req->netif;

	skb = cons_arp_req_packet(req->netif, &req->ip_addr);

	list_add_tail(&netif->out_queue, &skb->pkt_list);
	(*netif->transmit)(netif);
}

/*
 * only be called under process context
 */
static BOOLEAN output_arp_request(
	ether_netif_t *pdev, ip_addr_t *ip_req, arp_entry_t * pentry
	)
{
	BOOLEAN ret;
	list_t *tmp;
	arp_req_t *req;
	unsigned long flags;
	int times;
	synctimer_t arp_timer;
	struct timeout_struct arp_tm_data;

try_again:
	if (list_empty(&arp_req_free_list))
	{
		sleepon(&req_free_wait);
		goto try_again;
	}

	tmp = arp_req_free_list.next;
	list_del1(tmp);

	req = list_entry(tmp, list, arp_req_t);
	req->arp_wait = NULL;
	memcpy(&req->ip_addr, ip_req, IP_ADDR_LEN);
	req->netif = pdev;

	lock(flags);
	list_add_tail(&arp_req_head, &req->list);
	unlock(flags);

	ret = FALSE;
	times = 0;

	// make sure timeout won't happen before sleeping, other place...
	while(times < 3){
		lock(flags);

		emit_req(req);
		init_arp_timer(
			(ARP_TIMEOUT) << times,
			req,
			&arp_tm_data,
			&arp_timer
		);
		times++;

		sleepon(&req->arp_wait);

		if(arp_tm_data.tmout){
			unlock(flags);
			continue;
		}

		sync_delete(&arp_timer);

		ret = get_arp_entry(ip_req, pentry);
		if(ret){
			unlock(flags);
			break;
		}

		unlock(flags);
		
		printk("strange things happened\n");
	}

	// cancel the request and add to the free list

	lock(flags);
	list_del1(&req->list);
	unlock(flags);
	
	list_add_tail(&arp_req_free_list, &req->list);

	wakeup(&req_free_wait);
	
	return ret;
}

// do with the arp packet
void input_arp_packet(skbuff_t *skb)
{
	unsigned char *ptr;
	arp_packet_t *parp;
	ip_addr_t ip;
	int op_type;
	mac_addr_t mac;
	mac_addr_t bmac;
	ether_netif_t *netif = (ether_netif_t *)skb->netif;
	
	ptr = sk_get_buff_ptr(skb);
	ptr += skb->data_ptr;	// point to the valid data

	parp = (arp_packet_t *)ptr;
	
	if (pcnet32_debug)
	{
		print_mac_addr(&parp->mac_sender);
		printk("\t");
		print_ip_addr(&parp->ip_sender);
		printk("\n");
	}

	get_ether_mac_addr(netif, &mac);
	get_ether_boardcast_addr(netif, &bmac);

	if (!mac_addr_equal(&parp->mac_dst, &mac) &&	\
		!mac_addr_equal(&parp->mac_dst, &bmac))
	{
		skb_free(skb);
		return;
	}

	get_netif_ip_addr((netif_t *)netif, &ip);
	op_type = ntohs(parp->op_type);

	switch (op_type)
	{
		case ARP_REQ:
		{
			// XXX: check mac_sender and ip_sender
			add_arp_entry(&parp->mac_sender, &parp->ip_sender);
			
			if (ip_eq(&parp->ip_sendback, &ip))
			{
				cons_reply_arp_packet(skb);

				list_add_tail(&netif->out_queue, &skb->pkt_list);
				/* call the driver */
				(*netif->transmit)(netif);

				return;
			}
			
			break;
		}
			
		case ARP_REPLY:
		{
			arp_req_t *req;
			list_t *tmp, *pos;

			list_foreach(tmp, pos, &arp_req_head){
				req = list_entry(tmp, list, arp_req_t);
				if(ip_eq(&req->ip_addr, &parp->ip_sender)){
					add_arp_entry(
						&parp->mac_sender,
						&parp->ip_sender
					);

					wakeup(&req->arp_wait);
				}
			}
			
			break;
		}

		default:
			break;
	}

	skb_free(skb);
}

/*
 * only be called under process context
 */
BOOLEAN get_mac_addr_by_arp(
	ether_netif_t *netif, ip_addr_t *ip, mac_addr_t * pmac
	)
{
	arp_entry_t entry;

	// XXX: if any other has send the same arp_request
	if(get_arp_entry(ip, &entry) || output_arp_request(netif, ip, &entry)){
		//*pmac = entry.mac_addr;
		memcpy(pmac, &entry.mac_addr, sizeof(mac_addr_t));
		return TRUE;
	}

	return FALSE;
}

static void cons_reply_arp_packet(skbuff_t * skb)
{
	unsigned char *ptr;
	arp_packet_t *rarp;
	ether_netif_t *netif = (ether_netif_t *)skb->netif;
	mac_addr_t mac;

	ptr = sk_get_buff_ptr(skb);
	ptr += skb->data_ptr;
	rarp = (arp_packet_t *)ptr;

	get_ether_mac_addr(netif, &mac);

	memcpy(&rarp->mac_dst, &rarp->mac_sender, MAC_ADDR_LEN);
	memcpy(&rarp->mac_src, &mac, MAC_ADDR_LEN);
	
	rarp->op_type = htons(ARP_REPLY);
	memcpy(&rarp->mac_sender, &mac, MAC_ADDR_LEN);
	memcpy(&rarp->ip_sender, &rarp->ip_sendback, IP_ADDR_LEN);
}

#if 0
/* test code */
void invoke_arp_req2(void)
{
	ip_addr_t ip = {{192, 168, 23, 1 }};
	mac_addr_t mac;
	int i;

	for (i = 0; i < MAX_ARP_REQ + 1; i++)
	{
		printk("get ip: ");
		print_ip_addr(&ip);
		
		if (get_mac_addr_by_arp(current_ether, &ip, &mac))
		{
			printk("\t OK \t");
			print_mac_addr(&mac);
			printk("\n");
		}
		else
		{
			printk("\t NOT OK\n");
		}

		ip.ip[3]++;
	}
	
}

void invoke_arp_req(void)
{
	ip_addr_t ip = {{192, 168, 23, 100 }};
	mac_addr_t mac;
	int i;

	for (i = 0; i < MAX_ARP_REQ + 1; i++)
	{
		printk("get ip: ");
		print_ip_addr(&ip);
		
		if (get_mac_addr_by_arp(current_ether, &ip, &mac))
		{
			printk("\t OK \t");
			print_mac_addr(&mac);
			printk("\n");
		}
		else
		{
			printk("\t NOT OK\n");
		}

		ip.ip[3]++;
	}
	
}
#endif
