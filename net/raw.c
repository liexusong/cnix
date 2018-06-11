#include <cnix/errno.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/wait.h>
#include <cnix/mm.h>
#include <cnix/net.h>

static list_t raw_icmp_skblist;
static list_t raw_socket_list;
static int raw_icmp_user_num;
static struct wait_queue * raw_icmp_queue;

void raw_init(void)
{
	list_head_init(&raw_icmp_skblist);
	list_head_init(&raw_socket_list);
	raw_icmp_user_num = 0;
	raw_icmp_queue = NULL;
}

static skbuff_t * dup_dgram(skbuff_t * skb_in)
{
	list_t * tmp, * pos;
	skbuff_t * head, * skb1;
	unsigned char * data, * data1;
	skbuff_t *t;

	head = NULL;

	foreach(tmp, pos, &skb_in->list)
		t = list_entry(tmp, list, skbuff_t);
		skb1 = skb_alloc(t->dlen, 0);
		if(!skb1){
			if(head)
				skb_free_packet(head);

			return NULL;
		}

		if(!head){
			head = skb1;
			list_head_init(&head->list);
		}else
			list_add_tail(&head->list, &skb1->list);

		skb1->netif = t->netif;
		skb1->flag = t->flag;
		skb1->dgram_len = t->dgram_len;
		skb1->dlen = t->dlen;
		skb1->data_ptr = t->data_ptr;

		data = sk_get_buff_ptr(t);
		data += t->data_ptr;

		data1 = sk_get_buff_ptr(skb1);
		data1 += skb1->data_ptr;

		memcpy(data1, data, t->dlen - t->data_ptr);
	endeach(&skb_in->list);

	return head;
}

void input_raw_packet(skbuff_t * skb, iphdr_t * ip)
{
	skbuff_t * skb1;
	
	switch(ip->protocol){
	case IPPROTO_ICMP:
		// nobody want raw data
		if(raw_icmp_user_num <= 0)
			break;

		skb1 = dup_dgram(skb);
		if(skb1){
			list_t * tmp, * pos;

			list_add_tail(&raw_icmp_skblist, &skb1->pkt_list);
			wakeup(&raw_icmp_queue);

			list_foreach(tmp, pos, &raw_socket_list){
				socket_t * sock;

				sock = list_entry(tmp, list, socket_t);
				select_wakeup(&sock->select, OPTYPE_READ);
			}
		}
		break;
	default:
		break;
	}
}

int raw_open(socket_t * sock)
{
	unsigned long flags;

	if(sock->protocol != IPPROTO_ICMP)
		DIE("BUG: cannot happen\n");

	lockb_all(flags);

	raw_icmp_user_num += 1;

	list_add_tail(&raw_socket_list, &sock->list);

	unlockb_all(flags);

	return 0;
}

int raw_read(
	socket_t * sock,
	struct sockaddr_in * addr, int * addr_len,
	char * buffer, size_t count,
	int flags
	)
{
	iphdr_t * ip;
	list_t * tmp;
	skbuff_t * skb, * skb1;
	unsigned long bflags;
	int len, readed, reading;
	unsigned char * data;

	if(sock->protocol != IPPROTO_ICMP)
		DIE("BUG: cannot happen\n");

	lockb_all(bflags);

try_again:
	if(list_empty(&raw_icmp_skblist)){
		current->sleep_spot = &raw_icmp_queue;
		sleepon(&raw_icmp_queue);

		if(anysig(current)){
			unlockb_all(bflags);
			return -EINTR;
		}

		goto try_again;
	}

	tmp = raw_icmp_skblist.next;
	list_del(tmp);

	unlockb_all(bflags);

	skb = list_entry(tmp, pkt_list, skbuff_t);
	SK_TO_IP_HDR(ip, skb);

	if(addr){
		addr->sin_family = AF_INET;
		addr->sin_port = 0;
		addr->sin_addr.s_addr = ip->saddr;
		memset(addr->sin_zero, 0, 8);

		if(!addr_len)
			DIE("BUG: cannot happen\n");
		*addr_len = sizeof(struct sockaddr_in);
	}

	len = ip->tot_len;
	if(len > count)
		len = count;

	/* turn host-byte-order to network-byte-order back */
	ip->tot_len = htons(ip->tot_len);
	ip->id = htons(ip->id);
	ip->frag_off = htons(ip->frag_off);

	readed = 0;
	data = (unsigned char *)ip;
	
	skb1 = skb;

	do{
		reading = len - readed;
		if(skb1->dlen - skb1->data_ptr < reading)
			reading = skb1->dlen - skb1->data_ptr;

		memcpy(&buffer[readed], data, reading);

		readed += reading;

		skb1 = list_entry(skb1->list.next, list, skbuff_t);

		data = sk_get_buff_ptr(skb1);
		data += skb1->data_ptr;
	}while(readed < len);

	skb_free_packet(skb);

	return readed;
}

int raw_write(
	socket_t * sock,
	struct sockaddr_in * addr,
	const char * buffer, size_t count,
	int flags
	)
{
	int len, hdata_len, left, max, ret;
	skbuff_t * head;
	unsigned char * data;
	netif_t * netif;

	if(sock->protocol != IPPROTO_ICMP)
		DIE("BUG: cannot happen\n");

	if((count > 0) && !buffer)
		DIE("BUG: cannot happen\n");

	if(!addr)
		return -EINVAL;

	if(ISANY(addr->sin_addr))
		return -EINVAL;

	netif = find_outif((ip_addr_t *)&addr->sin_addr.s_addr, NULL);
	if(!netif)
		return -EHOSTUNREACH;

	max = netif->netif_mtu + ETHER_LNK_HEAD_LEN;
	if(max > MAX_PACKET_SIZE)
		max = MAX_PACKET_SIZE;

	len =  ETHER_LNK_HEAD_LEN + IP_NORM_HEAD_LEN + count;
	if(len < max)
		hdata_len = count;
	else{
		len = max;
		hdata_len = max
			- (ETHER_LNK_HEAD_LEN + IP_NORM_HEAD_LEN);
		// align to 8
		hdata_len &= ~7;
	}

	left = count - hdata_len;

	head = skb_alloc(len, 1);

	head->dgram_len = count;
	head->data_ptr = head->dlen - hdata_len;

	data = sk_get_buff_ptr(head);
	data += head->data_ptr;

	memcpy(data, buffer, hdata_len);

	head->flag |= SK_PK_HEAD_DATA; // icmp header and data
	// head->netif = netif;

	while(left > 0){
		int data_len;
		skbuff_t * skb;

		len =  ETHER_LNK_HEAD_LEN + IP_NORM_HEAD_LEN + left;
		if(len < max)
			data_len = left;
		else{
			len = max;
			data_len = max
				- (ETHER_LNK_HEAD_LEN + IP_NORM_HEAD_LEN);
			// align to 8
			data_len &= ~7;
		}

		skb = skb_alloc(len, 1);
		skb->flag |= SK_PK_DATA;
		skb->data_ptr = skb->dlen - data_len;

		data = sk_get_buff_ptr(skb);
		data += skb->data_ptr;

		memcpy(data, &buffer[count - left], data_len);

		list_add_tail(&head->list, &skb->list);

		left -= data_len;
	}

	ret = output_ip_packet(
		head, (ip_addr_t *)&addr->sin_addr.s_addr, IPPROTO_ICMP);
	if(ret < 0)
		return ret;

	return count;
}

int raw_close(socket_t * sock)
{
	unsigned long flags;

	if(sock->protocol != IPPROTO_ICMP)
		DIE("BUG: cannot happen\n");

	lockb_all(flags);

	raw_icmp_user_num -= 1;
	if(raw_icmp_user_num <= 0){
		list_t * tmp, * pos;

		list_foreach(tmp, pos, &raw_icmp_skblist){
			skbuff_t * skb;

			list_del(tmp);

			skb = list_entry(tmp, pkt_list, skbuff_t);
			skb_free_packet(skb);
		}
	}

	list_del(&sock->list);

	unlockb_all(flags);

	return 0;
}

int raw_sendmsg(
	socket_t * sock, const struct msghdr * message, int flags
	)
{
	return -ENOTSUP;
}

int raw_recvmsg(
	socket_t * sock, struct msghdr * message, int flags
	)
{
	return -ENOTSUP;
}

BOOLEAN raw_can_read(socket_t * sock)
{
	BOOLEAN ret;
	unsigned long flags;

	if(sock->protocol != IPPROTO_ICMP)
		DIE("BUG: cannot happen\n");

	lockb_all(flags);
	ret = !list_empty(&raw_icmp_skblist);
	unlockb_all(flags);

	return ret;
}

BOOLEAN raw_can_write(socket_t * sock)
{
	return TRUE;
}
