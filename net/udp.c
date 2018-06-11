#include <cnix/errno.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/wait.h>
#include <cnix/mm.h>
#include <cnix/net.h>

static list_t udpserv_list;

void udp_init(void)
{
	list_head_init(&udpserv_list);
}

typedef struct pseudphdr{
	unsigned long srcip;
	unsigned long dstip;
	unsigned char res;
	unsigned char protocol;
	unsigned short len;
	udphdr_t udp;
}pseudphdr_t;

void input_udp_packet(skbuff_t * skb, iphdr_t * ip)
{
	udphdr_t * udp;
	udpserv_t * serv;
	list_t * tmp, * pos;
	pseudphdr_t * pseudp;

	if(skb->dgram_len < sizeof(udphdr_t)){
		skb_free_packet(skb);
		return;
	}

	// must have second skb
	if(skb->dlen - skb->data_ptr < sizeof(udphdr_t)){
		skbuff_t * skb1;

		printk("need testing, move udp data up\n");

		if(!(skb1 = skb_move_up(skb, sizeof(udphdr_t)))){
			skb_free_packet(skb);
			return;
		}

		skb = skb1;
	}

	SK_TO_UDP_HDR(udp, skb);

	// construct the pseudo udp header, used by udp_read
	skb->data_ptr -= sizeof(pseudphdr_t) - sizeof(udphdr_t);

	SK_TO_DATA(pseudp, skb, pseudphdr_t);

	pseudp->srcip = ip->saddr;
	pseudp->dstip = ip->daddr;
	pseudp->res = 0;
	pseudp->protocol = IPPROTO_UDP;
	pseudp->len = udp->len;

	if(udp->check && checksum(skb)){
		skb_free_packet(skb);
		return;
	}

	skb->data_ptr += sizeof(pseudphdr_t) - sizeof(udphdr_t);
	
	udp->len = ntohs(udp->len);

	if(ip->tot_len - (ip->ihl << 2) != udp->len){
		skb_free_packet(skb);
		return;
	}

	list_foreach(tmp, pos, &udpserv_list){
		serv = list_entry(tmp, list, udpserv_t);

		if(serv->srcaddr.sin_port == udp->dest){
			list_add_tail(&serv->rcvskb, &skb->pkt_list);
			wakeup(&serv->wait);
			select_wakeup(&serv->socket->select, OPTYPE_READ);
			return;
		}
	}

	// XXX: send a icmp to inform ...

	skb_free_packet(skb);
}

static udpserv_t * alloc_udpserv(socket_t * sock)
{
	udpserv_t * serv;

	serv = (udpserv_t *)kmalloc(sizeof(udpserv_t), PageWait);

	memset(serv, 0, sizeof(udpserv_t));

	serv->state = UDP_INIT;
	list_head_init(&serv->xmtskb);
	list_head_init(&serv->rcvskb);
	serv->wait = NULL;
	serv->socket = sock;

	list_init(&serv->list);

	return serv;
}

static void free_udpserv(udpserv_t * serv)
{
	unsigned long flags;

	lockb_all(flags);
	list_del(&serv->list);
	unlockb_all(flags);

	if(!list_empty(&serv->rcvskb)){
		skbuff_t * skb;
		list_t * tmp, * pos;

		list_foreach(tmp, pos, &serv->rcvskb){
			skb = list_entry(tmp, pkt_list, skbuff_t);
			list_del(&skb->pkt_list);
			skb_free_packet(skb);
		}
	}

	kfree(serv);
}

int udp_connect(socket_t * sock, struct sockaddr_in * addr)
{
	int port;
	udpserv_t * serv;
	unsigned long flags;

	// has called connect, write, or send, ...
	if(sock->userv){
		unsigned long flags;

		lockb_all(flags);
		sock->userv->dstaddr = *addr;
		unlockb_all(flags);

		return 0;
	}

	port = inet_port_alloc(UDPP);
	if(port < 0)
		return -EAGAIN;

	serv = alloc_udpserv(sock);

	serv->srcaddr.sin_family = AF_INET;
	serv->srcaddr.sin_port = htons((unsigned short)port);
	serv->srcaddr.sin_addr.s_addr = 0; // ANY ADDRESS
	memset(serv->srcaddr.sin_zero, 0, 8);

	serv->dstaddr = *addr;

	lockb_all(flags);
	list_add_head(&udpserv_list, &serv->list);
	unlockb_all(flags);

	sock->userv = serv;

	return 0;
}

int udp_bind(socket_t * sock, struct sockaddr_in * addr)
{
	udpserv_t * serv;
	unsigned long flags;

	if(sock->userv)
		return -EINVAL;

	// XXX: group broadcast
	if(inet_port_busy(UDPP, ntohs(addr->sin_port)))
		return -EADDRINUSE;

	inet_port_use(UDPP, ntohs(addr->sin_port));

	serv = alloc_udpserv(sock);
	serv->srcaddr = *addr;

	lockb_all(flags);
	list_add_head(&udpserv_list, &serv->list);
	unlockb_all(flags);

	sock->userv = serv;

	return 0;
}

int udp_close(socket_t * sock)
{
	udpserv_t * serv = sock->userv;

	if(serv){
		inet_port_free(UDPP, ntohs(serv->srcaddr.sin_port));
		free_udpserv(sock->userv);
	}

	return 0;
}

static int __udp_read(
	socket_t * sock,
	struct sockaddr_in * addr, int * addr_len,
	struct iovbuf * iovb,
	int * flags
	)
{
	udpserv_t * serv;
	list_t * tmp;
	skbuff_t * skb;
	unsigned char * data;
	udphdr_t * udp;
	pseudphdr_t * pseudp;
	int len, readed, reading;
	unsigned long bflags;
	skbuff_t * skb1;

	serv = sock->userv;
	if(!serv)
		return -EBADF;

	lockb_all(bflags);

try_again:
	// this happens when operation is executed like "ifconfig eth0 down"
	if(serv->state == UDP_BROKEN){
		unlockb_all(bflags);
		return -EIO; // XXX
	}

	if(list_empty(&serv->rcvskb)){
		if((*flags) & MSG_DONTWAIT){
			unlockb_all(bflags);
			return -EWOULDBLOCK;
		}

		current->sleep_spot = &serv->wait;
		sleepon(&serv->wait);

		if(anysig(current)){
			unlockb_all(bflags);
			return -EINTR;
		}

		goto try_again;
	}

	tmp = serv->rcvskb.next;
	if(!((*flags) & MSG_PEEK))
		list_del(tmp);

	unlockb_all(bflags);

	skb = list_entry(tmp, pkt_list, skbuff_t);

	skb->data_ptr -= sizeof(pseudphdr_t) - sizeof(udphdr_t);
	SK_TO_DATA(pseudp, skb, pseudphdr_t);
	skb->data_ptr += sizeof(pseudphdr_t) - sizeof(udphdr_t);

	SK_TO_UDP_HDR(udp, skb);

	if(addr){
		addr->sin_family = AF_INET;
		addr->sin_port = udp->source;
		addr->sin_addr.s_addr = pseudp->srcip;
		memset(addr->sin_zero, 0, 8);

		if(!addr_len)
			DIE("BUG: cannot happen\n");
		*addr_len = sizeof(struct sockaddr_in);
	}

	len = udp->len - sizeof(udphdr_t);	

	if(len > iovb->len){
		len = iovb->len;
		*flags |= MSG_TRUNC;
	}

	readed = 0;
	data = (unsigned char *)(udp + 1);

	// skip the udp header
	skb->data_ptr += sizeof(udphdr_t);
	skb->dgram_len -= sizeof(udphdr_t);
	
	skb1 = skb;

	do{
		reading = len - readed;
		if(skb1->dlen - skb1->data_ptr < reading)
			reading = skb1->dlen - skb1->data_ptr;

		copy_to_iov(data, iovb, reading);

		readed += reading;

		skb1 = list_entry(skb1->list.next, list, skbuff_t);

		data = sk_get_buff_ptr(skb1);
		data += skb1->data_ptr;
	}while(readed < len);

	skb_free_packet(skb);

	return readed;
}

int udp_read(
	socket_t * sock,
	struct sockaddr_in * addr, int * addr_len,
	char * buffer, size_t count,
	int flags
	)
{
	struct iovec iov;
	struct iovbuf iovb;

	iov.iov_base = buffer;
	iov.iov_len = count;

	iovb.len = count;
	iovb.iovnum = 1;
	iovb.iovcur = 0;
	iovb.iovoff = 0;
	iovb.iov = &iov;

	return __udp_read(sock, addr, addr_len, &iovb, &flags);
}

static int __udp_write(
	socket_t * sock,
	struct sockaddr_in * addr,
	struct iovbuf * iovb,
	int flags
	)
{
	int len, hdata_len, left, max, ret;
	skbuff_t * head;
	unsigned char * data;
	pseudphdr_t * udp;
	struct sockaddr_in * addr1 = addr;
	netif_t * netif;
	ip_addr_t ip_addr;
	udpserv_t * serv;

	serv = sock->userv;

	if(!addr1){
		if(!serv)
			return -EBADF;
		if(ISANY(serv->dstaddr.sin_addr) || !serv->dstaddr.sin_port)
			return -EBADF;

		addr1 = &serv->dstaddr;
	}else if(ISANY(addr1->sin_addr) || !addr1->sin_port)
		return -EINVAL;

	if(!serv){
		unsigned long bflags;
		int port = inet_port_alloc(UDPP);

		if(port < 0)
			return -EAGAIN;

		serv = alloc_udpserv(sock);

		serv->srcaddr.sin_family = AF_INET;
		serv->srcaddr.sin_port = htons((unsigned short)port);
		serv->srcaddr.sin_addr.s_addr = 0; // ANY ADDRESS
		memset(serv->srcaddr.sin_zero, 0, 8);

		// XXX: dstaddr doesn't matter
		lockb_all(bflags);
		list_add_head(&udpserv_list, &serv->list);
		unlockb_all(bflags);

		sock->userv = serv;
	}

	serv->dstaddr = *addr1;

	netif = find_outif((ip_addr_t *)&addr1->sin_addr.s_addr, NULL);
	if(!netif)
		return -EHOSTUNREACH;

	max = netif->netif_mtu + ETHER_LNK_HEAD_LEN;
	if(max > MAX_PACKET_SIZE)
		max = MAX_PACKET_SIZE;

	len = ETHER_LNK_HEAD_LEN + IP_NORM_HEAD_LEN + sizeof(udphdr_t) + iovb->len;
	if(len < max)
		hdata_len = iovb->len;
	else{
		len = max;
		hdata_len = max
			- (ETHER_LNK_HEAD_LEN + IP_NORM_HEAD_LEN + sizeof(udphdr_t));
		// align to 8
		hdata_len &= ~7;
	}

	left = iovb->len - hdata_len;

	head = skb_alloc(len, 1);

	head->dgram_len = iovb->len + sizeof(udphdr_t);
	head->data_ptr = head->dlen - hdata_len - sizeof(pseudphdr_t);

	data = sk_get_buff_ptr(head);
	udp = (pseudphdr_t *)&data[head->data_ptr];

	get_netif_ip_addr(netif, &ip_addr);
	udp->srcip = iptoul(&ip_addr);
	udp->dstip = addr1->sin_addr.s_addr;

	udp->res = 0;
	udp->protocol = IPPROTO_UDP;
	udp->len = htons(iovb->len + sizeof(udphdr_t));
	udp->udp.source = serv->srcaddr.sin_port;
	udp->udp.dest = addr1->sin_port;
	udp->udp.len = udp->len;
	udp->udp.check = 0;

	copy_from_iov(&data[head->dlen - hdata_len], iovb, hdata_len);

	head->flag |= SK_PK_HEAD_DATA; // udp header and data
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

		copy_from_iov(&data[skb->data_ptr], iovb, data_len);

		list_add_tail(&head->list, &skb->list);

		left -= data_len;
	}

	// last thing to do is checksum
	udp->udp.check = checksum(head);

	head->data_ptr = head->dlen - hdata_len - sizeof(udphdr_t);

	ret = output_ip_packet(
		head, (ip_addr_t *)&addr1->sin_addr.s_addr, IPPROTO_UDP);
	if(ret < 0)
		return ret;

	return iovb->len;
}

int udp_write(
	socket_t * sock,
	struct sockaddr_in * addr,
	const char * buffer, size_t count,
	int flags
	)
{
	struct iovec iov;
	struct iovbuf iovb;

	iov.iov_base = (char *)buffer;
	iov.iov_len = count;

	iovb.len = count;
	iovb.iovnum = 1;
	iovb.iovcur = 0;
	iovb.iovoff = 0;
	iovb.iov = &iov;

	return __udp_write(sock, addr, &iovb, flags);
}

int udp_sendmsg(
	socket_t * sock, const struct msghdr * message, int flags
	)
{
	int i, iovlen, total;
	struct iovec * iova, * iov;
	struct sockaddr_in * addr;
	struct iovbuf iovb;

	addr = (struct sockaddr_in *)message->msg_name;
	if(!addr)
		return -EINVAL;

	if(message->msg_namelen < sizeof(struct sockaddr_in))
		return -EINVAL;

	if(cklimit(addr) || ckoverflow(addr, sizeof(struct sockaddr_in)))
		return -EFAULT;

	iova = message->msg_iov;
	if(!iova)
		return -EINVAL;

	iovlen = message->msg_iovlen;
	if(iovlen < 0)
		return -EINVAL;

	if(cklimit(iova) || ckoverflow(iova, sizeof(struct iovec) * iovlen))
		return -EFAULT;
	
	total = 0;

	for(i = 0; i < iovlen; i++){
		iov = &iova[i];
		if(!iov
			|| cklimit(iov->iov_base)
			|| (iov->iov_len < 0)
			|| ckoverflow(iov->iov_base, iov->iov_len))
			return -EFAULT;

		total += iov->iov_len;
	}

	if(!total)
		return 0;

	iovb.len = total;
	iovb.iovnum = iovlen;
	iovb.iovcur = 0;
	iovb.iovoff = 0;
	iovb.iov = iova;

	return __udp_write(sock, addr, &iovb, flags);
}

int udp_recvmsg(
	socket_t * sock, struct msghdr * message, int flags
	)
{
	int i, iovlen, total;
	struct iovec * iova, * iov;
	struct sockaddr_in * addr;
	struct iovbuf iovb;

	addr = (struct sockaddr_in *)message->msg_name;
	if(!addr)
		return -EINVAL;

	if(message->msg_namelen < sizeof(struct sockaddr_in))
		return -EINVAL;

	if(cklimit(addr) || ckoverflow(addr, sizeof(struct sockaddr_in)))
		return -EFAULT;

	iova = message->msg_iov;
	if(!iova)
		return -EINVAL;

	iovlen = message->msg_iovlen;
	if(iovlen < 0)
		return -EINVAL;

	if(cklimit(iova) || ckoverflow(iova, sizeof(struct iovec) * iovlen))
		return -EFAULT;
	
	total = 0;

	for(i = 0; i < iovlen; i++){
		iov = &iova[i];
		if(!iov
			|| cklimit(iov->iov_base)
			|| (iov->iov_len < 0)
			|| ckoverflow(iov->iov_base, iov->iov_len))
			return -EFAULT;

		total += iov->iov_len;
	}

	if(!total)
		return 0;

	iovb.len = total;
	iovb.iovnum = iovlen;
	iovb.iovcur = 0;
	iovb.iovoff = 0;
	iovb.iov = iova;

	message->msg_flags |= flags;

	return __udp_read(
		sock, addr, &message->msg_namelen, &iovb, &message->msg_flags
		);
}

BOOLEAN udp_can_read(socket_t * sock)
{
	BOOLEAN ret;
	unsigned long flags;
	udpserv_t * serv = sock->userv;

	if(serv) // won't block, but get error if reading
		return TRUE;

	lockb_all(flags);

	if(serv->state == UDP_BROKEN){
		unlockb_all(flags);
		return TRUE;
	}

	ret = !list_empty(&serv->rcvskb);

	unlockb_all(flags);

	return ret;
}

BOOLEAN udp_can_write(socket_t * sock)
{
	return TRUE;
}
