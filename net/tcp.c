#include <cnix/errno.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/wait.h>
#include <cnix/mm.h>
#include <cnix/net.h>
#include <cnix/fs.h>
#include <cnix/driver.h>
#include <cnix/tcp.h>

static BOOLEAN do_with_tcp_option(skbuff_t * skb, tcpserv_t * serv, int hdrlen);

static int tcp_state_syn_sent(tcpserv_t * serv);
static int tcp_state_syn(tcpserv_t * serv);
static int tcp_state_listen(tcpserv_t * serv, struct sockaddr_in * addr);
static int tcp_confirm_syn(tcpserv_t * serv);

/*
 * run in bottom half, so needn't to lockb_all for check serv->state.
 * find below list in code to know to update some field in tcpserv_t.
 *   update xmtseq
 *   update xmtack
 *   update xmtwin
 *   update rcvseq
 *   update rcvack
 *   update rcvwin
 * if receving fin, state will change after sending ack in send_tcp_delay_ack.
 */
static BOOLEAN reasm_tcpskb(tcpserv_t * serv, skbuff_t * skb, tcphdr_t * tcp)
{
	int acked_len = 0;
	BOOLEAN fin, ret, ack;
	unsigned long ack_seq = ntohl(tcp->ack_seq);

	if(KEEPALIVE(serv))
		stop_delay_timer(serv);

	if(serv->state < TCP_SYN){
		if(!tcp->syn)
			return FALSE;

		switch(serv->state){
		case TCP_LISTEN:
			serv->synnum += 1;
			if(serv->synnum >= serv->backlog){
				//skb_free_packet(skb);
				return FALSE;
			}

			list_add_tail(&serv->rcvskblist,&skb->pkt_list);

			wakeup(&serv->rcvwait);
			wakeup_for_tcpsel(serv, OPTYPE_READ);
			return TRUE;
		case TCP_SYN_SENT:
			// only need one
			if(list_empty(&serv->rcvskblist))
				list_add_tail(
					&serv->rcvskblist,&skb->pkt_list
				);
			else{
				PRINTK("horrible thing\n");
				skb_free_packet(skb);
			}

			// wakeup many times, doesn't matter
			wakeup(&serv->rcvwait);
			wakeup_for_tcpsel(serv, OPTYPE_READ);
			return TRUE;
		default:
			break;
		}

		return FALSE;
	}

	// do with retransmited syn packet
	if(tcp->syn){
		skb_free_packet(skb);
		return TRUE;
	}

	// check rst first
	if(tcp->rst){
		skb_free_packet(skb);

		if((compare_seq(ack_seq, serv->xmtack) > 0)
			&& (compare_seq(ack_seq, serv->xmtseq) <= 0))
			return TRUE;

		// user has called close, don't judge by state
		// user will be still in TCP_ESTABLISHED if there are more
		// data to send when tcp_close is called
		if(SERVCLOSED(serv)){ 
			if(serv->socket)
				serv->socket->tserv = NULL;

			free_all_resource(serv);
		}else{ // free_all_resource?
			serv->state = TCP_CLOSED;
			if(ANYTIMER(serv))
				stop_delay_timer(serv);

			wakeup(&serv->rcvwait);
			wakeup(&serv->xmtwait);
//			printk("op_read op_write\n");
			wakeup_for_tcpsel(serv, OPTYPE_READ | OPTYPE_WRITE);
		}

		return TRUE;
	}

	ack = FALSE;

	if(tcp->ack){
		if(compare_seq(ack_seq, serv->xmtack) > 0){
			// XXX, ack_seq just equal to serv->xmtseq + 1, no more
			if(SERVCLOSED(serv) && (ack_seq == serv->xmtseq + 1)){
				ack_seq -= 1;
				if(XMTFIN_NOACK(serv))
					serv->xmtfin = 3; // fin has been acked
			}else{
				int v;

				v = compare_seq(ack_seq, serv->xmtseq);
				if(v > 0){
					printk("state(%d), ", serv->state);
					printk("wierd, ack_seq is too large\n");
					return FALSE;
				}

				if(serv->bxmturg){
					v = compare_seq(
						ack_seq, serv->xmturgseq
						);

					if(v >= 0){
						serv->bxmturg = FALSE;
						serv->xmturgseq = 0;
					}
				}
			}

			// update xmtack, xmtack just points to fin at last
			ack = TRUE;
			acked_len = ack_seq - serv->xmtack;
			serv->xmtack = ack_seq;
		}
	}else if(!tcp->fin) // no syn, rst, ack, fin, impossible
		return FALSE;

	switch(serv->state){
	case TCP_SYN:
		if(!ack){
			skb_free_packet(skb);
			return TRUE;
		}

		acked_len -= 1;
		serv->state = TCP_ESTABLISHED;
		wakeup(&serv->rcvwait); // XXX
		wakeup_for_tcpsel(serv, OPTYPE_READ);
		break;
	case TCP_CLOSE_WAIT:
		return FALSE;
	case TCP_LAST_ACK:
		if(ack)
			free_all_resource(serv);

		skb_free_packet(skb);
		return TRUE;
	case TCP_CLOSING:
		if(ack){
			serv->state = TCP_TIME_WAIT;
			if(ANYTIMER(serv))
				stop_delay_timer(serv);
			start_time_wait_timer(serv);
		}

		skb_free_packet(skb);
		return TRUE;
	case TCP_TIME_WAIT:
		return FALSE;
	default:
		break;
	}

	// TCP_FIN_WAIT_1, TCP_FIN_WAIT_2, TCP_ESTABLISHED, readable

	if(ack){ // the other side didn't send fin yet
		if(serv->state == TCP_FIN_WAIT_1)
			serv->state = TCP_FIN_WAIT_2;

		serv->xmtring.count -= acked_len;
		if(serv->xmtring.count < 0)
			DIE("BUG: cannot happen\n");

		wakeup(&serv->xmtwait);
//		printk("op_write\n");
		wakeup_for_tcpsel(serv, OPTYPE_WRITE);

		// if being in rexmting state, then delete rexmt timer
		if(REXMT(serv)){
			serv->rexmttimes = 0;
			stop_delay_timer(serv);
		}
	}

	ret = insert_tcp_packet(serv, skb, tcp, &fin);
	if(ret){
		wakeup(&serv->rcvwait);	
		wakeup_for_tcpsel(serv, OPTYPE_READ);
	}

	if(fin)
		serv->rcvfin = 1;
	else{
		// XXX: if user has called close, just turn into TCP_TIME_WAIT
		if(!serv->socket && (serv->state == TCP_FIN_WAIT_2)){
			serv->state = TCP_TIME_WAIT;
			if(ANYTIMER(serv))
				stop_delay_timer(serv);
			start_time_wait_timer(serv);

			return TRUE;
		}
	}

	if(NOTIMER(serv) || DELAY(serv))
		restart_delay_timer(serv);

	return TRUE;
}

void input_tcp_packet(skbuff_t * skb, iphdr_t * ip)
{
	tcphdr_t * tcp;
	tcpserv_t * serv;
	psetcphdr_t * psetcp;

	if(skb->dgram_len < sizeof(tcphdr_t)){
		skb_free_packet(skb);
		return;
	}

	if(skb->dlen - skb->data_ptr < sizeof(tcphdr_t)){
		skbuff_t * skb1;

		printk("need testing, move up tcp data\n");

		if(!(skb1 = skb_move_up(skb, sizeof(tcphdr_t)))){
			skb_free_packet(skb);
			return;
		}

		skb = skb1;
	}

	SK_TO_TCP_HDR(tcp, skb);

	skb->data_ptr -= sizeof(psetcphdr_t) - sizeof(tcphdr_t);

	SK_TO_DATA(psetcp, skb, psetcphdr_t);

	psetcp->srcip = ip->saddr;
	psetcp->dstip = ip->daddr;
	psetcp->res = 0;
	psetcp->protocol = IPPROTO_TCP;
	psetcp->len = htons(ip->tot_len - (ip->ihl << 2));

	if(checksum(skb)){
		skb_free_packet(skb);
		return;
	}

	skb->data_ptr += sizeof(psetcphdr_t) - sizeof(tcphdr_t);

	if(skb->dlen - skb->data_ptr < (tcp->doff << 2)){
		skbuff_t * skb1;

		printk("need testing, move up tcp data including option\n");

		if(!(skb1 = skb_move_up(skb, tcp->doff << 2))){
			skb_free_packet(skb);
			return;
		}

		skb = skb1;
	}

	serv = get_match_tcpserv(ip, tcp);
	if(serv && reasm_tcpskb(serv, skb, tcp))
		return;

	send_tcp_rst(tcp, ip);
	skb_free_packet(skb);
}

struct conn_timeout{
	int out;
	struct wait_queue ** wait;
};

void conn_timeout_callback(void * data)
{
	struct conn_timeout * timeout = (struct conn_timeout *)data;

	timeout->out = 1;
	wakeup(timeout->wait);
}

/*
 * caller should lockb_all(flags)
 * return value:
 *   -1 - need to wait
 *    0 - rflag is not suitable in some state
 *    1 - success
 */
static int tcp_can_do(tcpserv_t * serv, BOOLEAN rflag)
{
	if(serv->state < TCP_LISTEN)
		return 0;

	if(serv->state < TCP_SYN){
		// wait in syscall accept or connect
		if(!list_empty(&serv->rcvskblist))
			return 1;
	}else if(serv->state != TCP_SYN){
		if(rflag){
			// check rcvring first, important
			if(!ring_is_empty(&serv->rcvring))
				return 1;

			if(!READABLE(serv))
				return 0;
		}else{
			// check state first,
			if(!WRITABLE(serv))
				return 0;

			if(ring_is_avail(&serv->xmtring))
				return 1;
		}
	}

	return -1;
}

/*
 * -x: failure
 *  0: rflag is not suitable in some state
 *  1: success
 */
static int wait_response(tcpserv_t * serv, int clicks, BOOLEAN rflag)
{
	int ret;
	unsigned long flags;
	synctimer_t conn_timer;
	struct conn_timeout timeout;

	lockb_all(flags);

	if(serv->state < TCP_LISTEN){ // TCP_CLOSED
		unlockb_all(flags);
		return -ENOTCONN;
	}

	ret = tcp_can_do(serv, rflag);
	if(ret >= 0){
		unlockb_all(flags);
		return ret;
	}

	current->sleep_spot = rflag ? &serv->rcvwait : &serv->xmtwait;

	if(clicks > 0){
		timeout.out = 0;
		timeout.wait = current->sleep_spot;
		conn_timer.expire = clicks;
		conn_timer.data = &timeout;
		conn_timer.timerproc = conn_timeout_callback;
		synctimer_set(&conn_timer);
	}

	sleepon(current->sleep_spot);

	if(clicks > 0){
		if(timeout.out){
			unlockb_all(flags);
			return -EHOSTDOWN;
		}

		sync_delete(&conn_timer);
	}

	if(anysig(current)){
		unlockb_all(flags);
		return -EINTR;
	}

	unlockb_all(flags);

	return 1;
}

static BOOLEAN do_with_tcp_option(skbuff_t * skb, tcpserv_t * serv, int hdrlen)
{
	int optidx, optlen;
	unsigned short * mss;
	unsigned char * data = sk_get_buff_ptr(skb);

	optidx = skb->data_ptr + sizeof(tcphdr_t);
	while(optidx < skb->dlen){
		switch(data[optidx]){
		case TCPOPT_NOP:
			optlen = 1;
			break;
		case TCPOPT_MSS:
			optlen = 4;
			mss =  (unsigned short *)&data[optidx + 2];
			if(ntohs(*mss) > 0)
				serv->rcvmss = ntohs(*mss);
			break;
		case TCPOPT_WIN: // XXX
			optlen = 3;
			break;
		case TCPOPT_TSM:
			optlen = 10;
			break;
		case TCPOPT_END:
		default:
			return FALSE;
		}

		optidx += optlen;
	}

	return TRUE;
}

int tcp_connect(socket_t * sock, struct sockaddr_in * dstaddr)
{
	int port, ret, times;
	netif_t * netif;
	tcpserv_t * serv;
	tcp_param_t param;

	port = 0;
	serv = NULL;
	
	if(sock->tserv)
		return -EISCONN;

	netif = find_outif((ip_addr_t *)&dstaddr->sin_addr.s_addr, NULL);
	if(!netif)
		return -ENETUNREACH;

	port = inet_port_alloc(TCPP);
	if(port < 0)
		return -EAGAIN;

	serv = alloc_tcpserv(TRUE);

	sock->tserv = serv;
	serv->socket = sock;

	serv->srcaddr.sin_family = AF_INET;
	serv->srcaddr.sin_port = htons(port);

	serv->srcaddr.sin_addr.s_addr = iptoul(netif->netif_addr->ip_addr);
	serv->dstaddr = *dstaddr;

	serv->netif = netif;

	param.seq = serv->xmtseq;
	serv->xmtseq += 1; // update xmtseq, syn will take one byte
	param.ack_seq = 0;
	memset(&param.bits, 0, sizeof(fsrpau_t));
	param.bits.syn = 1;
	param.window = serv->xmtwin;
	param.mss = serv->xmtmss;

	times = 0;

	// change state first, and begin to receive packet
	serv->state = TCP_SYN_SENT;
	add_tcpserv(serv);

try_again:
	ret = cs_tcp_packet(serv, &param, NULL, 0, 0, 1);
	if(ret < 0){
		inet_port_free(TCPP, port);
		free_tcpserv(serv);
		sock->tserv = NULL; // make sure tcp_close won't free port again
		return ret;
	}	

	ret = wait_response(serv, HZ << times, TRUE);
	if(ret < 0){
		times++;
		if(ret != (-EINTR) && (times < 3))
			goto try_again;

		inet_port_free(TCPP, port);
		free_tcpserv(serv);
		sock->tserv = NULL;
		return ret;
	}

	if((ret = tcp_state_syn_sent(serv)) < 0){
		inet_port_free(TCPP, port);
		free_tcpserv(serv);
		sock->tserv = NULL;
		return ret;
	}

	return 0;
}

int tcp_bind(socket_t * sock, struct sockaddr_in * addr)
{
	tcpserv_t * serv;

	if(sock->tserv)
		return -EINVAL;

	if(inet_port_busy(TCPP, ntohs(addr->sin_port)))
		return -EADDRINUSE;

	inet_port_use(TCPP, ntohs(addr->sin_port));

	serv = alloc_tcpserv(FALSE);
	sock->tserv = serv;
	serv->socket = sock;

	serv->srcaddr = *addr;

	return 0;
}

int tcp_listen(socket_t * sock, int backlog)
{
	tcpserv_t * serv = sock->tserv;

	if(!serv || (serv->state != TCP_INIT)) // didn't bind
		return -EINVAL;

	serv->state = TCP_LISTEN;
	serv->backlog = (backlog > 0) ? backlog : 1;

	// after bind
	add_tcpserv(serv);

	return 0;
}

/*
 * return value:
 *   -x, failed
 *    0, try again for accept
 *    1, success
 */
static int accept_connect(
	socket_t * sock,
	struct sockaddr_in * srcaddr,
	struct sockaddr_in * dstaddr,
	unsigned long rcvack,
	unsigned short rcvwin
	)
{
	int ret;
	netif_t * netif;
	tcpserv_t * serv;

	if(sock->tserv) // already connected
		DIE("BUG: cannot happen\n");

	netif = find_outif((ip_addr_t *)&dstaddr->sin_addr.s_addr, NULL);
	if(!netif)
		return -ENETUNREACH;

	serv = alloc_tcpserv(TRUE);

	serv->rcvseq = serv->rcvack = rcvack;
	serv->rcvwin = rcvwin;

	serv->srcaddr = *srcaddr;
	serv->srcaddr.sin_addr.s_addr = iptoul(netif->netif_addr->ip_addr);
	serv->dstaddr = *dstaddr;

	sock->tserv = serv;
	serv->socket = sock;
	serv->netif = netif;

	// change state first, and begin to receive packet	
	serv->state = TCP_SYN;

	add_tcpserv(serv);

	if((ret = tcp_confirm_syn(serv)) <= 0)
		return ret;

	return 1;
}

int tcp_accept(socket_t * sock, struct sockaddr_in * addr, int * addr_len)
{
	int ret, error = 0;
	struct inode * inoptr;
	socket_t * newsock;
	struct sockaddr_in daddr;

try_again:
	ret = wait_response(sock->tserv, 0, TRUE);
	if(ret < 0)
		return ret;

	if(tcp_state_listen(sock->tserv, &daddr) < 0)
		goto try_again;

	inoptr = iget(NODEV, 0);

	newsock = sock_alloc(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	newsock->inode = inoptr;

	inoptr->i_mode = I_SOCK;
	inoptr->i_data = newsock;

	ret = fdget(0, O_RDWR, inoptr, &error);
	if(ret < 0){
		iput(inoptr); // iput will call socket_close
		return error;
	}

	error = accept_connect(
		newsock,
		&sock->tserv->srcaddr, &daddr,
		sock->tserv->rcvack, sock->tserv->rcvwin
		);
	if(error < 0){
		ffree(fget(ret)); // will call iput, iput will call tcp_close 
		current->file[ret] = NULL;
		return error;
	}

	// receive tcp rst packet
	if(!error){
		ffree(fget(ret)); // will call iput, iput will call tcp_close 
		current->file[ret] = NULL;
		return -ECONNREFUSED;
	}

	if(addr){
		*addr = daddr;
		*addr_len = sizeof(struct sockaddr_in);
	}

	inoptr->i_count++;
	iput(inoptr);

	return ret;
}

static int __tcp_read(
	socket_t * sock, char * buffer, size_t count, int * flags, int off
	)
{
	BOOLEAN del;
	tcpserv_t * serv;
	unsigned long bflags;
	int ret, readed, reading;

	serv = sock->tserv;	
	if(!serv)
		return -ENOTCONN;

	readed = 0;

	if((*flags) & MSG_OOB){
		lockb_all(bflags);

		if(!serv->rcvurgs){
			unlockb_all(bflags);
			return -EINVAL;
		}

		if(serv->rcvurgs < 2){
			unlockb_all(bflags);
			return -EWOULDBLOCK;
		}

		buffer[0] = serv->rcvurgc;

		if(!((*flags) & MSG_PEEK))
			serv->rcvurgs = 0;

		unlockb_all(bflags);

		return 1;
	}

	del = TRUE;
	if((*flags) & MSG_PEEK)
		del = FALSE;

	while(readed < count){
		ret = wait_response(serv, 0, TRUE);
		if(ret < 0)
			return ret;

		if(!ret)
			break;

		if((*flags) & MSG_DONTWAIT)
			break;

		reading = count - readed;

		lockb_all(bflags);

		reading = copy_from_ring(
			(unsigned char *)&buffer[readed],
			&serv->rcvring,
			reading,
			off,
			del
			);

		// update xmtwin
		serv->xmtwin += reading;	

		unlockb_all(bflags);

		if(reading <= 0)
			break;

		serv->xmtwin_update = 1;

		readed += reading;

		if(!((*flags) & MSG_WAITALL))
			break;
	}

	return readed;
}

int tcp_read(socket_t * sock, char * buffer, size_t count, int flags)
{
	return __tcp_read(sock, buffer, count, &flags, 0);
}

int tcp_recvmsg(
	socket_t * sock, struct msghdr * message, int flags
	)
{
	int i, iovlen, total, ret;
	struct iovec * iova, * iov;

	iova = message->msg_iov;
	if(!iova)
		return -EINVAL;

	iovlen = message->msg_iovlen;
	if(iovlen < 0)
		return -EINVAL;

	if(cklimit(iova) || ckoverflow(iova, sizeof(struct iovec) * iovlen))
		return -EFAULT;
	
	message->msg_flags |= flags;

	total = 0;

	for(i = 0; i < iovlen; i++){
		iov = &iova[i];
		if(!iov
			|| cklimit(iov->iov_base)
			|| (iov->iov_len < 0)
			|| ckoverflow(iov->iov_base, iov->iov_len))
			return -EFAULT;

		if(!iov->iov_len)
			continue;

		ret = __tcp_read(
			sock,
			iov->iov_base, iov->iov_len,
			&message->msg_flags,
			total
			);
		if(ret < 0)
			return ret;

		if(!ret)
			break;

		if(message->msg_flags & MSG_OOB) // only one byte
			break;

		total += ret;
	}

	return total;
}

int tcp_write(socket_t * sock, const char * buffer, size_t count, int flags)
{
	tcpserv_t * serv;
	unsigned long bflags;
	int ret, writed, writing;
	
	serv = sock->tserv;
	if(!serv)
		return -ENOTCONN;

	writed = 0;

	while(writed < count){
		ret = wait_response(sock->tserv, 0, FALSE);
		if(ret < 0)
			return ret;

		if(!ret)
			break;

		if(flags & MSG_DONTWAIT)
			break;

		writing = count - writed;

		lockb_all(bflags);

		writing = copy_to_ring(
			(unsigned char *)&buffer[writed],
			&serv->xmtring,
			writing
			);

		// the other side closed socket
		if(writing <= 0){
			unlockb_all(flags);
			break;
		}

		if(writing >= count - writed){ // completed copying
			if(flags & MSG_OOB){
				serv->bxmturg = TRUE;
				serv->xmturgseq
					= serv->xmtack + serv->xmtring.count;
			}
		}

		unlockb_all(bflags);

		writed += writing;

		lockb_all(bflags);
		if(!ring_is_avail(&serv->xmtring))
			restart_delay_timer(serv);
		unlockb_all(bflags);
	}

	lockb_all(bflags);
	restart_delay_timer(serv);
	unlockb_all(flags);

	return writed;
}

int tcp_sendmsg(
	socket_t * sock, const struct msghdr * message, int flags
	)
{
	struct iovec * iova, * iov;
	int i, iovlen, total, ret, flags1;

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

		flags1 = flags;
		if(flags1 & MSG_OOB){
			if(i < (iovlen - 1)) // only last byte will be OOB byte
				flags1 &= ~MSG_OOB;
		}

		ret = tcp_write(
			sock, iov->iov_base, iov->iov_len, flags1
			);
		if(ret < 0)
			return ret;

		if(!ret)
			break;

		total += iov->iov_len;
	}	

	return total;
}

/*
 * tcp_close will be called in state:
 *   TCP_ESTABLISHED	user call close
 *   TCP_CLOSE_WAIT	user call close
 *   TCP_FIN_WAIT_1	user call close when has gottn fin
 *   TCP_CLOSED		accept(get rst in TCP_SYN state)
 */
int tcp_close(socket_t * sock)
{
	tcpserv_t * serv = sock->tserv;

	if(serv){
		unsigned long flags;

		lockb_all(flags);

		if(serv->state == TCP_CLOSED){
			free_all_resource(sock->tserv);
			sock->tserv = NULL;
			unlockb_all(flags);
			return 0;
		}

		if(WRITABLE(serv)){
			serv->xmtfin = 1;
			restart_delay_timer(serv);
		}else if(serv->state == TCP_LISTEN)
			free_all_resource(serv);

		sock->tserv = NULL;
		serv->socket = NULL; // important, used to diff from shutdown

		unlockb_all(flags);	
	}	

	return 0;
}

/*
 * return value:
 *   -x, failed
 *    0, receive a tcp rst packet, try again for accept
 *    1, success
 */
static int tcp_confirm_syn(tcpserv_t * serv)
{
	int times, ret;
	tcp_param_t param;
	unsigned long flags;

	times = 0;

	// send ack
	param.seq = serv->xmtseq;
	serv->xmtseq += 1; // update xmtseq, syn will take one byte
	param.ack_seq = serv->rcvseq;

	memset(&param.bits, 0, sizeof(fsrpau_t));
	param.bits.ack = 1;
	param.bits.syn = 1;

	param.window = serv->xmtwin;
	param.mss = serv->xmtmss;

try_again:
	// don't care failure, we have timer to deal with this condition
	cs_tcp_packet(serv, &param, NULL, 0, 0, 1);

	ret = wait_response(serv, HZ << times, TRUE);
	if(ret < 0){
		if(ret == (-EINTR)) // check signal first
			return ret;

		// very special, does tty have the same condition?

		lockb_all(flags);	
		if(serv->state == TCP_SYN){ // only timeout
			unlockb_all(flags);
			times++;
			if(times < 3)
				goto try_again;

			return ret;
		}

		unlockb_all(flags);
	}

	// state will be changed in bottom half when dealing with TCP_SYN state
	ret = tcp_state_syn(serv);
	if(ret <= 0)
		return ret;

	return 1;
}

static void send_syn_ack(tcpserv_t * serv)
{
	tcp_param_t param;

	// send ack
	param.seq = serv->xmtseq;
	param.ack_seq = serv->rcvseq;

	memset(&param.bits, 0, sizeof(fsrpau_t));
	param.bits.ack = 1;

	param.window = serv->xmtwin;
	param.mss = 0;

	cs_tcp_packet(serv, &param, NULL, 0, 0, 1); // failure doesn't matter
}

static int tcp_state_syn_sent(tcpserv_t * serv)
{
	unsigned long flags;
	list_t * tmp;
	skbuff_t * skb;
	tcphdr_t * tcp;
	int hdrlen;
	int ack, ret;

	lockb_all(flags);

	if(list_empty(&serv->rcvskblist))
		DIE("BUG: cannot happne\n");

	tmp = serv->rcvskblist.next;
	list_del(tmp);	

	skb = list_entry(tmp, pkt_list, skbuff_t);

	SK_TO_TCP_HDR(tcp, skb);

	if(!tcp->syn){
		serv->state = TCP_CLOSED; // reject any skb
		unlockb_all(flags);

		skb_free_packet(skb);
		return -ECONNREFUSED;
	}

	ack = tcp->ack;

	if(ack){
		// maybe overflow, but no problem
		if((serv->xmtack + 1) != ntohl(tcp->ack_seq)){
			serv->state = TCP_CLOSED; // reject any skb
			unlockb_all(flags);

			skb_free_packet(skb);
			return -ECONNREFUSED;
		}

		// update xmtack
		serv->xmtack = ntohl(tcp->ack_seq);
	}

	serv->state = TCP_SYN; // change state here

	unlockb_all(flags);
	
	serv->rcvseq = ntohl(tcp->seq) + 1; // 1 for syn
	serv->rcvack = serv->rcvseq;

	serv->rcvwin = ntohs(tcp->window);

	hdrlen = tcp->doff << 2;
	if(hdrlen > sizeof(tcphdr_t))
		do_with_tcp_option(skb, serv, hdrlen);

	skb_free_packet(skb);

	if(ack){
		lockb_all(flags);
		serv->state = TCP_ESTABLISHED;
		unlockb_all(flags);

		send_syn_ack(serv);

		return 0;
	}

	if((ret = tcp_confirm_syn(serv)) < 0)
		return ret;

	if(!ret) // receive a tcp rst packet
		return -ECONNREFUSED;

	return 0;
}

int tcp_state_listen(tcpserv_t * serv, struct sockaddr_in * addr)
{
	unsigned long flags;
	list_t * tmp;
	skbuff_t * skb;
	tcphdr_t * tcp;
	unsigned char * data;
	psetcphdr_t * psetcp;

	lockb_all(flags);

	if(list_empty(&serv->rcvskblist)){
		unlockb_all(flags);
		return -EAGAIN;
	}

	tmp = serv->rcvskblist.next;
	list_del(tmp);

	serv->synnum -= 1;

	unlockb_all(flags);

	skb = list_entry(tmp, pkt_list, skbuff_t);

	SK_TO_TCP_HDR(tcp, skb);

	if(!tcp->syn){
		skb_free_packet(skb);
		return -1;
	}

	serv->rcvseq = ntohl(tcp->seq);
	serv->rcvack = serv->rcvseq + 1; // 1 for syn
	serv->rcvwin = ntohs(tcp->window);

	data = sk_get_buff_ptr(skb);
	data += skb->data_ptr - (sizeof(psetcphdr_t) - sizeof(tcphdr_t));
	psetcp = (psetcphdr_t *)data;

	addr->sin_family = AF_INET;
	addr->sin_port = tcp->source;
	addr->sin_addr.s_addr = psetcp->srcip;

	skb_free_packet(skb);

	return 0;
}

/*
 * will be called in connect and listen
 */
static int tcp_state_syn(tcpserv_t * serv)
{
	unsigned long flags;
	int ret = -ECONNREFUSED;

	lockb_all(flags);

	switch(serv->state){
	case TCP_ESTABLISHED:	// got ack
		ret = 1;
		break;
	case TCP_CLOSED:	// got rst
		ret = 0;
		break;
	default:
		break;
	}

	unlockb_all(flags);

	return ret;
}

int tcp_shutdown(socket_t * sock, int how)
{
	int ret = 0;
	unsigned long flags;
	tcpserv_t * serv = sock->tserv;

	if(!serv)
		return -ENOTCONN;

	lockb_all(flags);

	if(serv->state < TCP_SYN){
		unlockb_all(flags);
		return -ENOTCONN;
	}

	switch(how){
	case SHUT_RD:
		break;
	case SHUT_WR:
	case SHUT_RDWR:
		if(!WRITABLE(serv))
			break;

		serv->xmtfin = 1;
		restart_delay_timer(serv);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	unlockb_all(flags);

	return ret;
}

BOOLEAN tcp_can_read(socket_t * sock)
{
	int ret;
	unsigned long flags;
	tcpserv_t * serv = sock->tserv;

	lockb_all(flags);
	ret = tcp_can_do(serv, TRUE);
	unlockb_all(flags);

	return (ret >= 0);
}

BOOLEAN tcp_can_write(socket_t * sock)
{
	int ret;
	unsigned long flags;
	tcpserv_t * serv = sock->tserv;

	lockb_all(flags);
	ret = tcp_can_do(serv, FALSE);
	unlockb_all(flags);

	return (ret >= 0);
}

BOOLEAN tcp_can_except(socket_t * sock)
{
	unsigned long flags;
	tcpserv_t * serv = sock->tserv;

	lockb_all(flags);

	if(serv && (serv->rcvurgs > 0)){
		unlockb_all(flags);
		return TRUE;
	}

	unlockb_all(flags);

	return FALSE;
}
