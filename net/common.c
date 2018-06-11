#include <cnix/types.h>
#include <cnix/kernel.h>
#include <cnix/list.h>
#include <cnix/string.h>
#include <cnix/inode.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>
#include <cnix/net.h>

#define MAX_IP_HASH_NR		127
static list_t ip_hash_table[MAX_IP_HASH_NR];

#define hash_fn(x, y, z)	((x << 3 ) + (y << 2) + z)

unsigned short ntohs(u16_t val)
{
	unsigned short ret;

	ret = (val >> 8) & 0xFF;
	ret |= ((val & 0xFF) << 8);

	return ret;
}

unsigned long ntohl(u32_t val)
{
	u32_t ret;
	u8_t *pval;
	u8_t *pret;

	pval = (u8_t *)&val;
	pret = (u8_t *)&ret;

	pval += 4;
	
	*pret++ = *--pval;
	*pret++ = *--pval;
	*pret++ = *--pval;
	*pret++ = *--pval;

	return ret;
}

BOOLEAN ip_eq(const ip_addr_t *ip1, const ip_addr_t *ip2)
{
	int i;
	const u8_t *p1 = &ip1->ip[0];
	const u8_t *p2 = &ip2->ip[0];
	
	for (i = 0; i < IP_ADDR_LEN; i++)
	{
		if (p1[i] != p2[i])
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOLEAN mac_addr_equal(const mac_addr_t *src, const mac_addr_t *dst)
{
	int i;
	const u8_t *p1 = &src->mac_addr[0];
	const u8_t *p2 = &dst->mac_addr[0];
	
	if (src == dst)
	{
		return TRUE;
	}

	for (i = 0; i < MAC_ADDR_LEN; i++)
	{
		if (p1[i] != p2[i])
		{
			return FALSE;
		}
	}
	
	return TRUE;
}

void print_mac_addr(mac_addr_t *pmac)
{
	int i;
	u8_t *ptr = &pmac->mac_addr[0];

	for (i = 0; i < MAC_ADDR_LEN - 1; i++)
	{
		printk("%02x:", ptr[i] & 0xFF);
	}
	
	printk("%02x", ptr[i] & 0xFF);
}

void print_ip_addr(ip_addr_t *pip)
{
	int i;
	u8_t *ptr = &pip->ip[0];
	
	for (i = 0; i < IP_ADDR_LEN - 1; i++)
	{
		printk("%d.", ptr[i] & 0xFF);
	}

	printk("%d", ptr[i]);
}

void print_buff_ip_addr(char *buff, ip_addr_t *pip)
{
	int i;
	u8_t *ptr = &pip->ip[0];
	int len = 0;
	int n;
	
	for (i = 0; i < IP_ADDR_LEN - 1; i++)
	{
		n = sprintf(buff + len, "%d.", ptr[i] & 0xFF);
		len += n;
	}

	sprintf(buff + len, "%d", ptr[i]);
}

unsigned short checksum(skbuff_t * skb)
{
	int len;
	long sum = 0;
	unsigned char * data;
	skbuff_t * skb1;
	list_t * tmp, * pos;

	foreach(tmp, pos, &skb->list)
		skb1 = list_entry(tmp, list, skbuff_t);

		data = sk_get_buff_ptr(skb1);
		data += skb1->data_ptr;

		len = skb1->dlen - skb1->data_ptr;
		while(len > 1){
			sum += *((unsigned short *)data);

			data += 2;
			if(sum & 0x80000000)
				sum = (sum & 0xffff) + (sum >> 16);
			len -= 2;
		}

		if(len)
			sum += *data;
	endeach(&skb->list);

	while(sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

unsigned short ipchecksum(skbuff_t * skb, int len)
{
	long sum = 0;
	unsigned char * data;

	data = sk_get_buff_ptr(skb);
	data += skb->data_ptr;

	while(len > 1){
		sum += *((unsigned short *)data);
		data += 2;
		if(sum & 0x80000000)
			sum = (sum & 0xffff) + (sum >> 16);
		len -= 2;
	}

	if(len)
		sum += *data;

	while(sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

socket_t * sock_alloc(int family, int type, int protocol)
{
	socket_t * sock;

	sock = (socket_t *)kmalloc(sizeof(socket_t), PageWait);

	list_head_init(&sock->list);

	sock->family = AF_INET;
	sock->type = type;
	sock->protocol = protocol;
	sock->state = SOCK_INIT;

	sock->opt.so_broadcast = 0;
	sock->opt.so_debug = 0;
	sock->opt.so_dontroute = 0;
	sock->opt.so_keepalive = 0;
	sock->opt.so_oobinline = 0;
	sock->opt.so_reuseaddr = 0;
	sock->opt.so_reuseport = 0;
	sock->opt.so_error = 0;
	sock->opt.so_linger.l_onoff = 0;
	sock->opt.so_linger.l_linger = 0;
	sock->opt.so_rcvbuf = DEFAULT_RING_SIZE;
	sock->opt.so_sndbuf = DEFAULT_RING_SIZE;
	sock->opt.so_rcvlowat = 1;
	sock->opt.so_sndlowat = 2048;
	sock->opt.so_rcvtimeo.tv_sec = 0;
	sock->opt.so_rcvtimeo.tv_usec = 0;
	sock->opt.so_sndtimeo.tv_sec = 0;
	sock->opt.so_sndtimeo.tv_usec = 0;

	sock->inode = NULL;
	sock->u.serv = NULL;
	select_init(&sock->select);

	return sock;
}

void sock_free(socket_t * sock)
{
	kfree(sock);
}

#define MAX_PORT_NUM 65536
#define RESERVED_PORT_NUM 1024

static unsigned char * udp_port_bits;
static unsigned char * tcp_port_bits;
static int last_udp_bit, last_tcp_bit;

int inet_port_alloc(int type)
{
	unsigned long * bits;
	int * lastbit, bit, idx, n;

	if(type == UDPP){
		bits = (unsigned long *)udp_port_bits;
		lastbit = &last_udp_bit;
	}else{
		bits = (unsigned long *)tcp_port_bits;
		lastbit = &last_tcp_bit;
	}

	n = 0;
	idx = 0;

	do{
		idx = (*lastbit + n) / 32;
		if(idx >= (MAX_PORT_NUM / 32))
			idx = (RESERVED_PORT_NUM / 32);

		if(bits[idx] ^ 0xfffffffful)
			break;

		bits++;
		n += 32;
	}while(n < MAX_PORT_NUM);

	if(n >= MAX_PORT_NUM)
		return -1;

	bit = 0;

	while(bits[idx] & (1 << bit))
		bit++;

	bits[idx] |= 1 << bit;

	*lastbit = (idx + 1) * 32;
	if(*lastbit > MAX_PORT_NUM)
		*lastbit = RESERVED_PORT_NUM;

	return (idx * 32 + bit);
}

void inet_port_free(int type, unsigned short port)
{
	unsigned long * bits;

	if(type == UDPP)
		bits = (unsigned long *)udp_port_bits;
	else
		bits = (unsigned long *)tcp_port_bits;

	if(!(bits[port / 32] & (1 << (port % 32))))
		DIE("BUG: cannot happen\n");

	bits[port / 32] &= ~(1 << (port % 32));
}

int inet_port_use(int type, unsigned short port)
{
	unsigned long * bits;

	if(type == UDPP)
		bits = (unsigned long *)udp_port_bits;
	else
		bits = (unsigned long *)tcp_port_bits;

	if(bits[port / 32] & (1 << (port % 32)))
		return -1;

	bits[port / 32] |= 1 << (port % 32);

	return 0;
}

BOOLEAN inet_port_busy(int type, unsigned short port)
{
	unsigned long * bits;

	if(type == UDPP)
		bits = (unsigned long *)udp_port_bits;
	else
		bits = (unsigned long *)tcp_port_bits;

	if(bits[port / 32] & (1 << (port % 32)))
		return TRUE;

	return FALSE;
}

void init_ip_hash_table(void)
{
	int i;

	for (i = 0; i < MAX_IP_HASH_NR; i++)
	{
		list_init(&ip_hash_table[i]);
	}
}

void add_ip_hash_entry(skbuff_t *skb)
{
	unsigned long val;
	iphdr_t *ip;

	SK_TO_IP_HDR(ip, skb);

	val = hash_fn(ip->saddr, ip->daddr, ip->id);
	val %= MAX_IP_HASH_NR;

#ifdef IP_DEBUG
	{
		skbuff_t *tmp;

		tmp = get_ip_hash_entry(skb);
		if (tmp)
		{
			PANIC("ip entry already exist!\n");
		}
	}
#endif
	
	list_add_head(&ip_hash_table[val], &skb->pkt_list);
}

skbuff_t *get_ip_hash_entry(skbuff_t *skb)
{
	unsigned long val;
	list_t *head;
	list_t *pos;
	skbuff_t *tmp;
	iphdr_t *ip;
	iphdr_t *ip_tmp;

	SK_TO_IP_HDR(ip, skb);
	
	val = hash_fn(ip->saddr, ip->daddr, ip->id);
	val %= MAX_IP_HASH_NR;

	head = &ip_hash_table[val];

	list_foreach_quick(pos, head)
	{
		tmp = list_entry(pos, pkt_list, skbuff_t);
		SK_TO_IP_HDR(ip_tmp, tmp);

		if (ip->daddr == ip_tmp->daddr && \
			ip->saddr == ip_tmp->saddr && \
			ip->id == ip_tmp->id)
		{
			return tmp;
		}
	}
#if 0
	printk("no ip hash entry for: saddr = 0x%x, daddr = 0x%x, id = 0x%x\n",
		ip->saddr, ip->daddr, ip->id);
#endif	
	return NULL;
}

#define SKIP_TAB(x)			\
	do {				\
		int i;			\
		for (i = 0; i < x; i++) \
		{			\
			printk("\t");	\
		}			\
	} while (0)
		

void print_packet(skbuff_t *head, int level)
{
	list_t *tmp;
	list_t *pos;
	skbuff_t *t;
	iphdr_t *ip;
	int i;
	
	i = 0;
	foreach (tmp, pos, &head->list)
	{
		t = list_entry(tmp, list, skbuff_t );

		SKIP_TAB(level);	
		printk("no.(%d): data_ptr = %d, dlen = %d, dgram_len = %d\n",
			i, t->data_ptr, t->dlen, t->dgram_len);
		if (t->flag & (SK_PK_HEAD | SK_PK_HEAD_DATA))
		{
			SK_TO_IP_HDR(ip, t);
			
			SKIP_TAB(level);
			printk("ip ver = %d, ip ihl = %d\n ", ip->version, ip->ihl);
			SKIP_TAB(level);
			printk("ip tot_len = %d, ip id = %d\n ",
				ntohs(ip->tot_len) & 0xFFFF, 
				ntohs(ip->id) & 0xFFFF);
			SKIP_TAB(level);
			printk("ip frag_off = %d, ", (ntohs(ip->frag_off) & FRAG_MASK) * 8);
			printk("%s\n", (ntohs(ip->frag_off) & IP_MORE) ? "F" : "NF");
		}
		
		i++;
	}
	endeach(&head->list);
}

void print_packets(skbuff_t *head)
{
	list_t *tmp;
	list_t *pos;
	skbuff_t *t;
	int i;

	i = 0;
	foreach (tmp,  pos, &head->pkt_list)
	{
		t = list_entry(tmp, pkt_list, skbuff_t);

		printk("packet no.(%d), dgram_len = %d:\n", i, t->dgram_len);
		print_packet(t, 1);
		i++;
	}
	endeach(&head->pkt_list);
}

int get_queue_count(list_t *head)
{
	int n = 0;
	
	list_t *tmp;
	list_t *pos;

	list_foreach(tmp, pos, head)
	{
		n++;
	}

	return n;
}

void netmask_ip(const ip_addr_t *ip_addr, 
		const ip_addr_t *mask, 
		ip_addr_t *ip_ret)
{
	int i;

	for (i = 0; i < IP_ADDR_LEN; i++)
	{
		ip_ret->ip[i] = ip_addr->ip[i] & mask->ip[i];
	}
}

/*
 * caller must make sure ivob has enough data to copy
 */
int copy_from_iov(unsigned char * buffer, struct iovbuf * iovb, int len)
{
	char * base;
	int i, n, copied;
	struct iovec * iov;

	copied = 0;

	for(i = iovb->iovcur; (copied < len) && (i < iovb->iovnum); i++){
		iov = &iovb->iov[i];

		n = len - copied;
		if(n > iov->iov_len - iovb->iovoff)
			n = iov->iov_len - iovb->iovoff;

		base = (char *)iov->iov_base;
		memcpy(&buffer[copied], &base[iovb->iovoff], n);

		copied += n;
		iovb->iovoff = iov->iov_len - n;
	}

	if(copied < len)
		DIE("BUG: cannot happen\n");

	iovb->iovcur = i;

	return copied;
}

/*
 * caller must make sure ivob has enough data to copy
 */
int copy_to_iov(unsigned char * buffer, struct iovbuf * iovb, int len)
{
	char * base;
	int i, n, copied;
	struct iovec * iov;

	copied = 0;

	for(i = iovb->iovcur; (copied < len) && (i < iovb->iovnum); i++){
		iov = &iovb->iov[i];

		n = len - copied;
		if(n > iov->iov_len - iovb->iovoff)
			n = iov->iov_len - iovb->iovoff;

		base = (char *)iov->iov_base;
		memcpy(&base[iovb->iovoff], &buffer[copied], n);

		copied += n;
		iovb->iovoff = iov->iov_len - n;
	}

	if(copied < len)
		DIE("BUG: cannot happen\n");

	iovb->iovcur = i;

	return copied;
}

int compare_seq(unsigned long seq, unsigned long seq2)
{
	int delta, seq1 = 0x00010000;

	delta = seq1 - seq;
	seq2 += delta;

	if(seq1 < seq2)
		return -1;

	if(seq1 > seq2)
		return 1;

	return 0;
}

extern void skbuff_init(void);
extern void pcnet32_init(void);
extern void raw_init(void);
extern void udp_init(void);
extern void tcp_init(void);

#define BITPAGES (MAX_PORT_NUM / (PAGE_SIZE * 8))

void net_init(void)
{
	int order = 0;

	while((1 << order) < BITPAGES)
		order++;

	udp_port_bits = (unsigned char *)get_free_pages(0, order);
	tcp_port_bits = (unsigned char *)get_free_pages(0, order);
	if(!udp_port_bits || !tcp_port_bits)
		DIE("BUG: need to modify code\n");

	last_udp_bit = last_tcp_bit = RESERVED_PORT_NUM;

	skbuff_init();

	loopback_init();
	pcnet32_init();

	//
	init_route();

	init_arp_queue();
	init_ip_hash_table();
	raw_init();
	udp_init();
	tcp_init();
}
