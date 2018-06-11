#include <cnix/errno.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/wait.h>
#include <cnix/mm.h>
#include <cnix/net.h>
#include <cnix/fs.h>
#include <cnix/driver.h>
#include <cnix/tcp.h>

static list_t tcpserv_list;
static unsigned long tcp_sequence; // increment two in 10ms interval
static synctimer_t tcp_seq_timer;

static void start_tcp_seq_timer(void);

#if TCP_TEST
void tcp_server(void * data)
{
	int fd, socklen;
	socket_t * server;
	struct sockaddr_in saddr, caddr;
	char buffer[32];

	strcpy(current->myname, "tcp_server");

	server = sock_alloc(AF_INET, SOCK_STREAM, 0);

	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(8888);
	saddr.sin_addr.s_addr = 0;

	if(tcp_bind(server, &saddr) < 0)
		DIE("BUG: cannot happen\n");

	if(tcp_listen(server, 5) < 0)
		DIE("BUG: cannot happen\n");

	socklen = sizeof(struct sockaddr_in);

	// check server's rcvskblist
	fd = tcp_accept(
		server,
		(struct sockaddr_in *)&caddr,
		&socklen
		);
	if(fd < 0)
		DIE("BUG: cannot happen\n");

	if(do_read(fd, buffer, 21) > 0){
		buffer[21] = '\0';
		printk("get message: %s\n", buffer);
	}else
		printk("get no message\n");

	do_close(fd);

	tcp_close(server);
	sock_free(server);

	printk("tcp testing passed, please reboot\n");

	kernel_exit(0);
}

static void send_tcp_packet(tcpserv_t * serv, int len, int xmtfin)
{
	tcp_param_t param;

	memset(&param.bits, 0, sizeof(fsrpau_t));

	param.seq = serv->xmtseq;
	if(XMTFIN_ACKED(serv) && serv->rcvfin){
		if(len > 0)
			DIE("BUG: cannot happen\n");

		param.seq += 1;
	}

	param.bits.ack = 1;
	param.ack_seq = serv->rcvseq;

	param.bits.fin = xmtfin;

	param.window = serv->xmtwin;
	param.mss = 0; // not upate mss

	cs_tcp_packet(serv, &param, &serv->xmtring, 0, len, 0);

	// update rcvack
	serv->rcvack = serv->rcvseq;
}

static void fill_ring_with_seq(
	tcpserv_t * serv, unsigned long seq, char * buffer, int len
	)
{
	copy_to_ring((unsigned char *)buffer, &serv->xmtring, len);
	serv->xmtseq = seq;
	send_tcp_packet(serv, len, 0);
	serv->xmtring.count -= len;
}

void tcp_client(void * data)
{
	int ret;
	socket_t * client;
	struct sockaddr_in caddr;
	tcpserv_t * serv;
	unsigned long seqbak;

	strcpy(current->myname, "tcp_client");

	client = sock_alloc(AF_INET, SOCK_STREAM, 0);

	caddr.sin_family = AF_INET;
	caddr.sin_port = htons(8888);
	caddr.sin_addr.s_addr = 192 | (168 << 8) | (23 << 16) | (123 << 24);

	if((ret = tcp_connect(client, &caddr)) < 0){
		printk("connect failed(%d) ", ret);
		printk("need to set IP address manually\n");
		kernel_exit(0);
	}

	serv = client->tserv;
	seqbak = serv->xmtseq;

	// send packets
	fill_ring_with_seq(serv, seqbak + 10, "hello", 5);
	fill_ring_with_seq(serv, seqbak + 1, "abcd", 4);
	fill_ring_with_seq(serv, seqbak + 5, "world", 5);
	fill_ring_with_seq(serv, seqbak + 11, "elloplease", 10);
	fill_ring_with_seq(serv, seqbak, "h", 1);

	serv->xmtseq = seqbak + 21;

	printk("client xmtseq %x\n", seqbak);

	tcp_close(client);
	sock_free(client);

	kernel_exit(0);
}
#endif

void tcp_init(void)
{
	list_head_init(&tcpserv_list);

	tcp_sequence = curclock();
	start_tcp_seq_timer();
}

static void tcp_seq_timer_callback(void * data)
{
	tcp_sequence += 2;
	start_tcp_seq_timer();
}

#if (HZ % 100)
#error "HZ is not correct"
#endif

static void start_tcp_seq_timer(void)
{
	tcp_seq_timer.expire = (HZ / 100); // 10MS
	tcp_seq_timer.data = &tcp_sequence;
	tcp_seq_timer.timerproc = tcp_seq_timer_callback;
	synctimer_set(&tcp_seq_timer);
}

/*
 * run in bottom half, needn't lockb_all
 */
static BOOLEAN send_tcp_delay_ack(
	tcpserv_t * serv,
	unsigned long xmtseq,
	BOOLEAN fromhead,
	int len,
	int xmtfin
	)
{
	int off;
	tcp_param_t param;

	memset(&param.bits, 0, sizeof(fsrpau_t));

	param.seq = xmtseq;
	if(XMTFIN_ACKED(serv) && serv->rcvfin){
		if(len > 0)
			DIE("BUG: cannot happen\n");

		param.seq += 1;
	}

	param.bits.ack = 1;
	param.ack_seq = serv->rcvseq;

	param.bits.fin = xmtfin;

	param.window = serv->xmtwin;
	param.mss = 0; // not upate mss

	if(fromhead)
		off = 0;
	else{
		off = serv->xmtseq - serv->xmtack;
		if(off < 0)
			DIE("BUG: cannot happen\n");
	}

	if(cs_tcp_packet(serv, &param, &serv->xmtring, off, len, 0) < 0)
		return FALSE;

	// update rcvack
	serv->rcvack = serv->rcvseq;

	serv->xmtwin_update = 0;

	return TRUE;
}

static void rcvfin_switch_state(tcpserv_t * serv)
{
	int state = 0;

	switch(serv->state){
	case TCP_ESTABLISHED:
		state = TCP_CLOSE_WAIT;
		wakeup(&serv->rcvwait);
		wakeup_for_tcpsel(serv, OPTYPE_READ);
		break;
	case TCP_FIN_WAIT_1:
		state = TCP_CLOSING;
		break;
	case TCP_FIN_WAIT_2:
		state = TCP_TIME_WAIT;
		if(ANYTIMER(serv))
			stop_delay_timer(serv);
		start_time_wait_timer(serv);
		break;
	default:
		return;
	}

	serv->state = state;
}

static void xmtfin_switch_state(tcpserv_t * serv)
{
	int state = 0;

	switch(serv->state){
	case TCP_ESTABLISHED:
	case TCP_SYN:
		state = TCP_FIN_WAIT_1;
		break;
	case TCP_CLOSE_WAIT:
		state = TCP_LAST_ACK;
		break;
	default:
		return;
	}

	serv->state = state;
}

static void __start_delay_timer(
	tcpserv_t * serv, int timertype, void (*callback)(void *), int n
	)
{
	if(ANYTIMER(serv) && (serv->timertype != timertype)){
		printk(
			"in timer %d, to start timer %d, wrong operation\n",
			serv->timertype,
			timertype
		);
		return;
	}

	serv->timertype = timertype;

	serv->delaytimer.expire = (HZ / 5) * n;
	serv->delaytimer.data = serv;
	serv->delaytimer.timerproc = callback;

	synctimer_set(&serv->delaytimer);
}

static void rexmit_timer_callback(void * data)
{
	int len, xmtfin = 0;
	unsigned long xmtseq;
	tcpserv_t * serv = (tcpserv_t *)data;

	if(!REXMT(serv))
		DIE("BUG: cannot happen\n");

	if(serv->rexmttimes > 16){ // after try 16 times
		serv->state = TCP_CLOSED; // disconnect directly
		return;
	}

	xmtseq = serv->xmtseq - serv->xmtring.count;

	// rexmt don't care rcvwin

	len = serv->xmtring.count;
	if(len > serv->rcvmss)
		len = serv->rcvmss;
	else if((len <= 0) && XMTFIN_NOACK(serv)){
		// data and fin, don't send together
		xmtfin = 1;
	}

	serv->rexmttimes++; // always try

	send_tcp_delay_ack(serv, xmtseq, TRUE, len, xmtfin);

	// exponential backoff
	__start_delay_timer(
		serv, 2, rexmit_timer_callback, 7 << (serv->rexmttimes & 7)
		);
}

static void start_rexmt_timer(tcpserv_t * serv)
{
	if(ANYTIMER(serv))
		DIE("BUG: cannot happen\n");

	// rexmt after 1.4s
	__start_delay_timer(serv, 2, rexmit_timer_callback, 7);
}

static void keepalive_timer_callback(void * data)
{
	tcpserv_t * serv = (tcpserv_t *)data;

	if(!KEEPALIVE(serv))
		DIE("BUG: cannot happen\n");

	if(serv->keepalive_count > 96 + 10){
		serv->state = TCP_CLOSED; // disconnect directly
		return;
	}

	if(serv->keepalive_count > 96)
		send_tcp_delay_ack(serv, serv->xmtseq, FALSE, 0, 0);

	serv->keepalive_count += 1;
	__start_delay_timer(serv,
		4, keepalive_timer_callback, 375); // 75 seconds
}

static void start_keepalive_timer(tcpserv_t * serv)
{
	if(ANYTIMER(serv))
		DIE("BUG: cannot happen\n");

	serv->keepalive_count = 0;
	__start_delay_timer(serv,
		4, keepalive_timer_callback, 36000); // two hours
}

/*
 * be called when get new data(include fin) or have data to send
 */
void delay_timer_callback(void * data)
{
	int len, unack_len, xmtfin;
	tcpserv_t * serv = (tcpserv_t *)data;

	serv->timertype = 0;

	if(serv->xmtseq - serv->xmtack < 0) // unacked data
		DIE("BUG: cannot happen\n");

	unack_len = serv->xmtseq - serv->xmtack;

	len = serv->xmtring.count - unack_len;
	if(len > serv->rcvmss)
		len = serv->rcvmss;

try_again:
	xmtfin = 0;

	if(serv->rcvwin <= unack_len){
		 // do nothing if any data to send except fin
		if(len > 0){
			// persistent timer, just to ask rcvwin
			send_tcp_delay_ack(serv, serv->xmtack, FALSE, 0, 0);

			// 60 seconds
			__start_delay_timer(serv, 3, delay_timer_callback, 300);
			return;
		}
	}else if(len > serv->rcvwin - unack_len)
		len = serv->rcvwin - unack_len;

	if(len <= 0){ // data and fin, don't send together
		if(XMTFIN(serv))
			xmtfin = 1;
		else if(!RCVFIN(serv)){	
			// maybe we got a persistent timer request
			if(serv->xmtwin_update)
				send_tcp_delay_ack(
					serv, serv->xmtseq, FALSE, 0, 0
				);

			if(serv->xmtseq != serv->xmtack){ // rexmit data
				serv->timercount += 1;

				if(serv->timercount > 3){
					serv->timercount = 0;
					start_rexmt_timer(serv);
				}else
					start_delay_timer(serv);
			}else
				start_keepalive_timer(serv);

			return;
		}
	}

	if(!send_tcp_delay_ack(serv, serv->xmtseq, FALSE, len, xmtfin)){ 
		printk("this message just for debuging\n");
		start_delay_timer(serv); // retransmit after
		return;
	}

	// update xmtseq, xmtseq just points to fin at last
	serv->xmtseq += len;

	// check rcvfin first
	if(RCVFIN(serv)){
		rcvfin_switch_state(serv);
		if(serv->state == TCP_TIME_WAIT) // it's over
			return;

		serv->rcvfin = 2;
	}

	if(xmtfin){
		serv->xmtfin = 2;
		xmtfin_switch_state(serv);
	}

	unack_len = serv->xmtseq - serv->xmtack;
	len = serv->xmtring.count - unack_len;

	if(len > serv->rcvmss){
		len = serv->rcvmss;
		goto try_again;
	}	

	start_delay_timer(serv);
}

/*
 * caller must lockb_all(flags)
 */
void start_delay_timer(tcpserv_t * serv)
{
	__start_delay_timer(serv, 1, delay_timer_callback, 1);
}

/*
 * caller must lockb_all(flags)
 */
void restart_delay_timer(tcpserv_t * serv)
{
	if(!REXMT(serv)){
		if(ANYTIMER(serv))
			stop_delay_timer(serv);

		if(serv->timercount > 0)
			serv->timercount -= 1;

		// don't call delay_timer_callback directly,
		// give user some time to empty rcvring,
		// but delay less than 200MS
		serv->timertype = 1;

		serv->delaytimer.expire = (HZ / 100); // 10MS
		serv->delaytimer.data = serv;
		serv->delaytimer.timerproc = delay_timer_callback;

		synctimer_set(&serv->delaytimer);
	}
}

/*
 * caller must lockb_all(flags)
 */
void stop_delay_timer(tcpserv_t * serv)
{
	if(NOTIMER(serv))
		DIE("BUG: cannot happen\n");

	serv->timertype = 0;
	sync_delete(&serv->delaytimer);
}

static void time_wait_callback(void * data)
{
	tcpserv_t * serv = (tcpserv_t *)data;

	if(serv->socket) // for second shutdown or close after shutdown
		serv->socket->tserv = NULL; 

	free_all_resource(serv);
}

void start_time_wait_timer(tcpserv_t * serv)
{
	unsigned long flags;

	lockb_all(flags);

	if(ANYTIMER(serv)) // delete timer first
		stop_delay_timer(serv);

	serv->timertype = 5; // timewait timer

	serv->delaytimer.expire = HZ * 120; // 2 minutes;
	serv->delaytimer.data = serv;
	serv->delaytimer.timerproc = time_wait_callback;

	synctimer_set(&serv->delaytimer);

	unlockb_all(flags);
}

/*
 * construct and send tcp packet
 */
int cs_tcp_packet(
	tcpserv_t * serv,
	tcp_param_t * param,
	ring_buffer_t * ring,	// data to send
	int off,		// offset for xmtring to copy from
	int data_len,		// length to copy
	int wait
	)
{
	int max, len, optlen, left, hdata_len, ret;
	skbuff_t * head;
	unsigned char * data;
	tcphdr_t * tcp1;
	psetcphdr_t * psetcp1;
	unsigned long flags;

	if(!serv->netif)
		DIE("BUG: cannot happen\n");

	if((data_len > 0) && !ring)
		DIE("BUG: cannot happen\n");

	max = serv->netif->netif_mtu + LINK_HEAD_LEN;
	if(max > MAX_PACKET_SIZE)
		max = MAX_PACKET_SIZE;

	optlen = 0;
	if(param->mss > 0) // XXX
		optlen = 4;

	len = LINK_HEAD_LEN + IP_NORM_HEAD_LEN
		+ sizeof(tcphdr_t) + optlen + data_len;

	if(len < max)
		hdata_len = data_len;
	else{
		len = max;
		hdata_len =
			max
			-
			(
				LINK_HEAD_LEN + IP_NORM_HEAD_LEN
				+ sizeof(tcphdr_t) + optlen
			);
	}

	left = data_len - hdata_len;

	head = skb_alloc(len, wait);
	if(!head)
		return -ENOMEM; // XXX

	head->dgram_len = data_len + sizeof(tcphdr_t) + optlen;
	head->data_ptr = head->dlen - hdata_len - sizeof(psetcphdr_t) - optlen;

	data = sk_get_buff_ptr(head);
	psetcp1 = (psetcphdr_t *)&data[head->data_ptr];

	psetcp1->srcip = serv->srcaddr.sin_addr.s_addr;
	psetcp1->dstip = serv->dstaddr.sin_addr.s_addr;
	psetcp1->res = 0;
	psetcp1->protocol = IPPROTO_TCP;
	psetcp1->len = htons(data_len + sizeof(tcphdr_t) + optlen);

	tcp1 = &psetcp1->tcp;

	tcp1->source = serv->srcaddr.sin_port;
	tcp1->dest = serv->dstaddr.sin_port;
	tcp1->seq = htonl(param->seq);

	if(param->bits.ack)
		tcp1->ack_seq = htonl(param->ack_seq);
	else
		tcp1->ack_seq = 0;

	tcp1->res1 = 0;
	tcp1->doff = (sizeof(tcphdr_t) + optlen) >> 2;

	tcp1->fin = param->bits.fin;
	tcp1->syn = param->bits.syn;
	tcp1->rst = param->bits.rst;

	if(data_len > 0)
		tcp1->psh = 1; // always true
	else
		tcp1->psh = 0;

	tcp1->ack = param->bits.ack;

	if(serv->bxmturg && (compare_seq(serv->xmturgseq, param->seq) > 0)){
		tcp1->urg = 1;
		tcp1->urg_ptr = htons(serv->xmturgseq - param->seq);
	}else{
		tcp1->urg = 0;
		tcp1->urg_ptr = 0;
	}

	tcp1->res2 = 0;
	tcp1->window = htons(param->window);
	tcp1->check = 0;

	if(hdata_len > 0){ // XXX
		lockb_all(flags);
		if(copy_from_ring(&data[head->dlen - hdata_len],
			ring, hdata_len, off, FALSE) < hdata_len)
			DIE("BUG: cannot happen\n");
		unlockb_all(flags);
	}

	if(param->mss > 0){ 
		int optidx = head->dlen - hdata_len - optlen;

		data[optidx++] = TCPOPT_MSS;
		data[optidx++] = 4; // length
		*((unsigned short *)&data[optidx])
			= (unsigned short)htons(param->mss);
	}

	if(data_len)
		head->flag |= SK_PK_HEAD_DATA;
	else
		head->flag |= SK_PK_HEAD;

	while(left > 0){
		int len1;
		skbuff_t * skb;

		len =  LINK_HEAD_LEN + IP_NORM_HEAD_LEN + left;
		if(len < max)
			len1 = left;
		else{
			len = max;
			len1 = max
				- (LINK_HEAD_LEN + IP_NORM_HEAD_LEN);
		}

		skb = skb_alloc(len, 1);
		skb->flag |= SK_PK_DATA;
		skb->data_ptr = skb->dlen - len1;
		data = sk_get_buff_ptr(skb);

		lockb_all(flags);
		if(copy_from_ring(&data[skb->data_ptr],
			ring, len1, off + data_len - left, FALSE) < len1)
			DIE("BUG: cannot happen\n");
		unlockb_all(flags);

		list_add_tail(&head->list, &skb->list);

		left -= len1;
	}

	tcp1->check = checksum(head);

	head->data_ptr = head->dlen - hdata_len - sizeof(tcphdr_t) - optlen;

	{
		ip_addr_t dstip = ultoip(serv->dstaddr.sin_addr.s_addr);

		if(wait){
			ret = output_ip_packet(head, &dstip, IPPROTO_TCP);
			if(ret < 0)
				return ret;
		}else // no wait
			add_reply_list(head, &dstip, IPPROTO_TCP);
	}

	return 0;
}

static int match_socket(tcpserv_t * serv, iphdr_t * ip, tcphdr_t * tcp)
{
	int value = 4;
	struct sockaddr_in * src, * dest;

	src = &serv->srcaddr;

	if(src->sin_port != tcp->dest)
		return 0;

	if(src->sin_addr.s_addr != ip->daddr){
		if(serv->state > TCP_LISTEN)
			return 0;

		if(ISANY(src->sin_addr))
			value -= 1;
	}

	dest = &serv->dstaddr;

	if(dest->sin_port != tcp->source){
		if(serv->state > TCP_LISTEN)
			return 0;

		if(!dest->sin_port)
			value -= 1;
	}

	if(dest->sin_addr.s_addr != ip->saddr){
		if(serv->state > TCP_LISTEN)
			return 0;

		if(ISANY(dest->sin_addr))
			value -= 1;
	}

	return value;
}

/*
 * TODO:
 *    use hash. the same to udp.
 */
tcpserv_t * get_match_tcpserv(iphdr_t * ip, tcphdr_t * tcp)
{
	int ret, value = 0;
	list_t * tmp, * pos;
	tcpserv_t * serv, * serv1;

	serv = NULL;

	list_foreach(tmp, pos, &tcpserv_list){
		serv1 = list_entry(tmp, list, tcpserv_t);

		ret = match_socket(serv1, ip, tcp);
		if(ret > value){
			value = ret;
			serv = serv1;
			if(value >= 4)
				break;
		}
	}

	if(serv && (tcpserv_list.next != &serv->list)){ // LRU, other place
		list_del(&serv->list);
		list_add_head(&tcpserv_list, &serv->list);
	}

	return serv;
}

void send_tcp_rst(tcphdr_t * tcp, iphdr_t * ip)
{
	tcpserv_t serv;
	tcp_param_t param;
	unsigned long ack_seq;

	serv.netif = find_outif((ip_addr_t *)&ip->saddr, NULL);
	if(!serv.netif)
		return;

	serv.srcaddr.sin_addr.s_addr = ip->daddr;
	serv.dstaddr.sin_addr.s_addr = ip->saddr;
	serv.srcaddr.sin_port = tcp->dest;
	serv.dstaddr.sin_port = tcp->source;
	param.seq = ntohl(tcp->ack_seq);
	ack_seq = ntohl(tcp->seq)
		+ ip->tot_len 
		- (ip->ihl << 2) - (tcp->doff << 2)
		+ tcp->syn + tcp->fin
		;
	param.ack_seq = ack_seq;
	memset(&param.bits, 0, sizeof(fsrpau_t));
	param.bits.rst = 1;
	param.bits.ack = 1;
	param.window = 0;
	param.mss = 0;

	cs_tcp_packet(&serv, &param, NULL, 0, 0, 0);
}

#define OOBINLINE(serv) \
	(serv->socket && serv->socket->opt.so_oobinline)

/*
 * skb has been trimmed tcp header
 */
static int skb_copy_data(skbuff_t * skb, tcpserv_t * serv, int urg_ptr)
{
	skbuff_t * tmp;
	ring_buffer_t * ring;
	unsigned char * data;
	int copying, copied, len;

	ring = &serv->rcvring;

	if(skb->dgram_len > ring->size - ring->count)
		return 0;

	copied = 0;
	tmp = skb;

	do{
		// it's possible tmp->dlen equal to tmp->data_ptr
		if(tmp->dlen - tmp->data_ptr > 0){
			copying = tmp->dlen - tmp->data_ptr;

			data = sk_get_buff_ptr(tmp);
			data += tmp->data_ptr;

			if(!OOBINLINE(serv)
				&& (urg_ptr > 0)
				&& (copied <= (urg_ptr - 1))
				&& ((copied + copying) > (urg_ptr - 1))){
				int len1 = (urg_ptr - 1) - copied;

				serv->rcvurgc = data[len1];
				if(len1 > 0)
					copy_to_ring(data, ring, len1);

				len = copy_to_ring(
					&data[len1], ring, copying - 1 - len1
					);

				len += len1 + 1;
			}else
				len = copy_to_ring(data, ring, copying);

			if(len < copying) // out of ring buffer
				break;

			copied += len;
		}

		tmp = list_entry(tmp->list.next, list, skbuff_t);
	}while(tmp != skb);

	return copied;
}

/*
 * cut from head, include tcp head
 * but the head skb is reserved, so head maybe contain no data
 */
static void skb_cut_data(skbuff_t * skb, int count)
{
	int hdrlen;
	skbuff_t * skb1;
	tcphdr_t * tcp;
	list_t * tmp, * pos;

	SK_TO_TCP_HDR(tcp, skb);

	hdrlen = tcp->doff << 2;

	if(skb->dgram_len - hdrlen < count)
		DIE("BUG: cannot happen\n");

	skb->dgram_len -= hdrlen + count;
	skb->data_ptr += hdrlen; // cut tcp header first

	if(skb->dlen - skb->data_ptr >= count)
		skb->data_ptr += count;
	else{
		int len = count - (skb->dlen - skb->data_ptr);

		// cut the first skb, no data, but keep skb
		skb->data_ptr = skb->dlen;

		list_foreach(tmp, pos, &skb->list){
			if(len <= 0)
				break;

			skb1 = list_entry(tmp, list, skbuff_t);		
			if(skb1->dlen - skb1->data_ptr > len){
				skb1->data_ptr += len;
				return;
			}

			len -= skb1->dlen - skb1->data_ptr;

			// no data, free this skb
			list_del(&skb1->list);
			skb_free(skb1);
		}

		if(len > 0)
			DIE("BUG: cannot happen\n");
	}
}

static void link_skb(skbuff_t * skb, skbuff_t * skb1)
{
	list_t * skblast;

	skb->list.prev->next = &skb1->list;
	skb1->list.prev->next = &skb->list;

	skblast = skb->list.prev;

	skb->list.prev = skb1->list.prev;
	skb1->list.prev = skblast;
}

/*
 * return value:
 *   TRUE, inserted successfully
 *   FALSE, needn't to insert, free skb packet
 */
static BOOLEAN real_insert_tcp_packet(
	tcpserv_t * serv,
	skbuff_t * skb, unsigned long seq, unsigned long seq_end, int delta,
	tcphdr_t * tcp
	)
{
	list_t * tmp, * pos;
	skbuff_t * skb1;
	unsigned long seq1, seq_end1;
	tcphdr_t * tcp1;

	list_foreach(tmp, pos, &serv->rcvskblist){
		skb1 = list_entry(tmp, pkt_list, skbuff_t);

		SK_TO_TCP_HDR(tcp1, skb1);

		seq1 = ntohl(tcp1->seq) + delta;
		seq_end1 = seq1 + skb1->dgram_len - (tcp1->doff << 2);
		if(tcp1->fin)
			seq_end1 += 1;

		if(seq < seq1){ // insert before skb1
			if(seq_end < seq1){ // not overlaped data
				list_insert(tmp->prev, &skb->pkt_list);
				return TRUE;
			}

			if(seq_end >= seq_end1){ // skb1 is included in skb
				list_del(tmp);
				skb_free_packet(skb1);
				continue;
			}

			if(tcp->fin){ // skb cannot contain the last byte
				PRINTK("cannot happen this condition\n");
				return FALSE;
			}

			tcp->fin = tcp1->fin;

			// cut overlapped data from skb1, include tcp header
			skb_cut_data(skb1, seq_end - seq1);	

			// merge skb1 to skb
			skb->dgram_len += skb1->dgram_len;
			seq_end += skb1->dgram_len + tcp1->fin; // XXX

			list_del(tmp);

			// link skb1 to skb
			link_skb(skb, skb1);
			continue;
		}

		// seq >= seq1
		if(seq_end <= seq_end1) // skb is included in skb1
			return FALSE;

		// seq_end > seq_end1, skb1 is included in skb
		if(seq == seq1){ // discard old one
			list_insert(tmp, &skb->pkt_list);
			list_del(tmp);
			return TRUE;
		}

		// seq > seq1, seq_end > seq_end1
		if(seq <= seq_end1){
			printk("special, test carefully\n");

			if(tcp1->fin){ // skb1 cannot contain the last byte
				PRINTK("cannot happen\n this condition\n");
				return FALSE;
			}

			tcp1->fin = tcp->fin;

			skb_cut_data(skb, seq_end1 - seq);

			// merge skb1 to skb
			skb1->dgram_len += skb->dgram_len;
			seq = seq1;

			// delete from rcvskblist
			list_del(tmp);

			// link skb to skb1
			link_skb(skb1, skb);

			skb = skb1;
			tcp = tcp1;
		}
	}

	list_add_tail(&serv->rcvskblist, &skb->pkt_list);

	return TRUE;
}

/*
 * return value:
 *   TRUE, new data needs to ack
 *   FALSE, no new data needs to ack
 */
BOOLEAN insert_tcp_packet(
	tcpserv_t * serv, skbuff_t * skb, tcphdr_t * tcp, BOOLEAN * fin
	)
{
	/*
	 * |<-------------rcvbuffsize--------------->|
	 * +++++++ data +++++++|+++++++ window +++++++
	 *    |<-rcvack  ++++->|rcvseq
	 *    0x00010000
	 */

	int hdrlen, len;
	unsigned short urg_ptr;
	unsigned long seq, seq_end, rcvseq, delta;

	*fin = FALSE;

	hdrlen = tcp->doff << 2;

	if(skb->dgram_len < hdrlen){ // no data
		skb_free_packet(skb);
		return FALSE;
	}

	// take 0x00010000 as the origin point
	rcvseq = 0x00010000;
	delta = rcvseq - serv->rcvseq;

	seq = ntohl(tcp->seq) + delta;

	seq_end = seq + skb->dgram_len - hdrlen;
	if(tcp->fin)
		seq_end += 1;

	if(seq_end <= rcvseq){	
		if(seq_end < rcvseq){
			skb_free_packet(skb);
			return FALSE;
		}

		if(PERSISTENT(serv)){
			if(serv->rcvwin < ntohs(tcp->window)){
				stop_delay_timer(serv);
				restart_delay_timer(serv);
			}
		}

		// update rcvwin
		serv->rcvwin = ntohs(tcp->window);

		skb_free_packet(skb);
		return FALSE;
	}

	if(serv->xmtwin < skb->dgram_len - hdrlen){
		printk("something wrong with the other side's tcp/ip stack\n");
		skb_free_packet(skb);
		return FALSE;
	}

	// put skb into rcvskblist, if no new data, free skb packet
	if(!real_insert_tcp_packet(serv, skb, seq, seq_end, delta, tcp)){
		skb_free_packet(skb);
		return FALSE;
	}

	skb = list_entry(serv->rcvskblist.next, pkt_list, skbuff_t);

	SK_TO_TCP_HDR(tcp, skb);

	// recaculate hdrlen, seq, seq_end
	hdrlen = tcp->doff << 2;
	seq = ntohl(tcp->seq) + delta;
	seq_end = seq + skb->dgram_len - hdrlen;
	if(tcp->fin)
		seq_end += 1;

	// copy data to rcvbuff
	if(seq <= rcvseq){
		if(seq_end <= rcvseq)
			DIE("BUG: cannot happen\n");

		if(PERSISTENT(serv)){
			if(serv->rcvwin < ntohs(tcp->window)){
				stop_delay_timer(serv);
				restart_delay_timer(serv);
			}
		}

		// update rcvwin
		serv->rcvwin = ntohs(tcp->window);

		//if(!do_with_tcp_option(skb, serv, tcp->doff << 2))
		//	return FALSE;

		// update rcvseq 
		if(tcp->fin){
			*fin = TRUE;
			serv->rcvseq += 1;
		}

		// trim tcp header
		skb->data_ptr += hdrlen;
		skb->dgram_len -= hdrlen;

		if(skb->dgram_len <= 0){ // only fin
			if(!(*fin))
				DIE("BUG: cannot happen\n");

			return FALSE;
		}

		// update xmtwin
		serv->xmtwin -= skb->dgram_len;

		urg_ptr = 0;

		if(!OOBINLINE(serv)
			&& tcp->urg && ((urg_ptr = ntohs(tcp->urg_ptr)) > 0)){
			if(urg_ptr <= skb->dgram_len){
				serv->rcvurgs = 2; // urg ptr, and urg data
				serv->xmtwin += 1; // update xmtwin
			}else{ // no urg data
				serv->rcvurgs = 1; // only urg ptr
				urg_ptr = 0;
			}

			// XXX: test it
			wakeup_for_tcpsel(serv, OPTYPE_EXCEPT);
		}else
			serv->rcvurgs = 0; // no urg

		len = skb_copy_data(skb, serv, urg_ptr);
		if(len > 0) // update rcvseq 
			serv->rcvseq += len;
		else
			PRINTK("must be something wrong with window\n");

		list_del(&skb->pkt_list);
		skb_free_packet(skb);

		return TRUE;
	}

	return FALSE;
}

void free_all_resource(tcpserv_t * serv)
{
	if(!serv)
		DIE("BUG: cannot happen\n");

	tcp_free_port(serv);
	free_tcpserv(serv);
}

tcpserv_t * alloc_tcpserv(BOOLEAN need)
{
	tcpserv_t * serv;

	serv = (tcpserv_t *)kmalloc(sizeof(tcpserv_t), PageWait);

	memset(serv, 0, sizeof(tcpserv_t)); // clear srcaddr, dstaddr

	serv->state = TCP_INIT;
	serv->xmtfin = 0;
	serv->rcvfin = 0;

	if(need){
		init_ring(&serv->xmtring, DEFAULT_RING_SIZE);
		init_ring(&serv->rcvring, DEFAULT_RING_SIZE);
	}else{
		init_ring(&serv->xmtring, 0);
		init_ring(&serv->rcvring, 0);
	}

	list_head_init(&serv->rcvskblist);

	serv->timertype = 0;
	serv->timercount = 0;

	serv->rexmttimes = 0;

	serv->keepalive_count = 0;

	serv->xmtwait = NULL;
	serv->rcvwait = NULL;

	serv->xmtseq = serv->xmtack = tcp_sequence;
	serv->rcvseq = serv->rcvack = 0;

	tcp_sequence += 64000;

	serv->xmtwin_update = 0;

	// XXX, no matter what's serv->rcvring.size
	serv->xmtwin = DEFAULT_RING_SIZE; 
	serv->rcvwin = 0;

	serv->xmtmss = XMT_MSS;
	serv->rcvmss = RCV_MSS;

	serv->backlog = 0;
	serv->synnum = 0;

	serv->rcvurgs = 0;
	serv->rcvurgc = 0;

	serv->bxmturg = FALSE;
	serv->xmturgseq = 0;

	list_init(&serv->list);

	return serv;
}

void add_tcpserv(tcpserv_t * serv)
{
	unsigned long flags;

	lockb_all(flags);
	list_add_head(&tcpserv_list, &serv->list);
	unlockb_all(flags);
}

void tcp_free_port(tcpserv_t * serv)
{
	int found = 0;
	list_t * tmp, * pos;
	unsigned short port;
	tcpserv_t * serv1;
	unsigned long flags;

	lockb_all(flags);

	list_foreach(tmp, pos, &tcpserv_list){
		serv1 = list_entry(tmp, list, tcpserv_t);
		port = serv1->srcaddr.sin_port;
		if((serv1 != serv) && (serv->srcaddr.sin_port == port)){
			found = 1;
			break;
		}
	}

	unlockb_all(flags);

	if(!found)
		inet_port_free(TCPP, ntohs(serv->srcaddr.sin_port));
}

void free_tcpserv(tcpserv_t * serv)
{
	unsigned long flags;

	lockb_all(flags);

	list_del(&serv->list);
	if(ANYTIMER(serv))
		stop_delay_timer(serv);

	unlockb_all(flags);

	if(!list_empty(&serv->rcvskblist)){
		skbuff_t * skb;
		list_t * tmp, * pos;

		list_foreach(tmp, pos, &serv->rcvskblist){
			list_del(tmp);
			skb = list_entry(tmp, pkt_list, skbuff_t);
			skb_free_packet(skb);
		}
	}

	if(serv->xmtring.data)
		kfree(serv->xmtring.data);

	if(serv->rcvring.data)
		kfree(serv->rcvring.data);

	kfree(serv);
}

void wakeup_for_tcpsel(tcpserv_t * serv, short optype)
{
	socket_t * sock = serv->socket;

	if(sock)
		select_wakeup(&sock->select, optype);
}

static int printip(unsigned long * pip)
{
	int i, len = 0;
	u8_t *ptr = (u8_t *)pip;
	
	for (i = 0; i < IP_ADDR_LEN - 1; i++)
	{
		len += printk("%d.", ptr[i] & 0xFF);
	}

	len += printk("%d", ptr[i]);

	return len;
}

void print_netstat(void)
{
	int len;
	list_t * tmp, * pos;
	tcpserv_t * serv = NULL;
	char * states[] = {
		"closed", "listen", "synsent", "syn", "closewait",
		"lastack", "established", "finwait1", "finwait2",
		"closing", "timewait"
	};

	printk("\nladdr\t\tlport\t\tfaddr\t\tfport\t\tstate\n");

	list_foreach(tmp, pos, &tcpserv_list){
		serv = list_entry(tmp, list, tcpserv_t);

		len = printip(&serv->srcaddr.sin_addr.s_addr);
		if(len < 8)
			printk("\t");

		printk("\t%d\t\t", ntohs(serv->srcaddr.sin_port));

		len = printip(&serv->dstaddr.sin_addr.s_addr);
		if(len < 8)
			printk("\t");

		printk("\t%d\t\t", ntohs(serv->dstaddr.sin_port));

		printk("%s\n", states[serv->state]);
	}
}
