#ifndef _SOCKET_H
#define _SOCKET_H

#include <cnix/list.h>
#include <cnix/skbuff.h>
#include <cnix/inode.h>
#include <cnix/driver.h>
#include <cnix/ringbuff.h>
#include <cnix/sol.h>
#include <cnix/select.h>
#include <cnix/time.h>

#define AF_UNSPEC	0
#define AF_UNIX		1
#define AF_INET		2

#define SOCK_STREAM	1
#define SOCK_DGRAM	2
#define SOCK_RAW	3

#define MSG_OOB		0x01
#define MSG_PEEK	0x02
#define MSG_DONTROUTE	0x04
#define MSG_CTRUNC	0x08
#define MSG_PROXY	0x10
#define MSG_TRUNC	0x20
#define MSG_DONTWAIT	0x40
#define MSG_EOR		0x80
#define MSG_WAITALL	0x100

#define SHUT_RD		0
#define SHUT_WR		1
#define SHUT_RDWR	2

#define IP_TOS		1
#define IP_TTL		2
#define IP_HDRINCL	3
#define IP_OPTIONS	4

#define	TCP_NODELAY	 1
#define	TCP_MAXSEG	 2

struct _netif;

struct in_addr{
	unsigned long s_addr;
};

#define INADDR_ANY 0
#define ISANY(addr) ((addr.s_addr) == INADDR_ANY)

struct sockaddr_in{
	short sin_family;
	unsigned short sin_port;
	struct in_addr sin_addr;
	unsigned char sin_zero[8];
};

struct sockaddr{
	unsigned short sa_family;
	char sa_data[14];
};

struct iovec{
	void * iov_base;
	int iov_len;
};

struct iovbuf{
	int len;	// total length of iovec
	int iovnum;	// number of iovec
	int iovcur;	// current iovec
	int iovoff;	// offset in current iovec
	struct iovec * iov;
};

struct cmsghdr{
	int cmsg_len;
	int cmsg_level;
	int cmsg_type;
	unsigned char cmsg_data[0];
};

struct msghdr{
	void * msg_name;
	int msg_namelen;
	struct iovec * msg_iov;
	int msg_iovlen;
	void * msg_control;
	int msng_controllen;
	int msg_flags;
};

struct socket;

typedef struct udpserv{
	list_t list;
	int state;
	list_t xmtskb, rcvskb; // xmtskb, never used
	struct wait_queue * wait;
	struct sockaddr_in srcaddr, dstaddr;
	struct socket * socket;
}udpserv_t;

#define UDP_INIT	0
#define UDP_BROKEN	1

/* don't change the order of state */
enum{
	TCP_INIT,
	TCP_CLOSED = TCP_INIT,	// 0
	TCP_LISTEN,		// 1
	TCP_SYN_SENT,		// 2

	TCP_SYN,		// 3
	TCP_CLOSE_WAIT,		// 4
	TCP_LAST_ACK,		// 5

	TCP_ESTABLISHED,	// 6

	TCP_FIN_WAIT_1,		// 7
	TCP_FIN_WAIT_2,		// 8
	TCP_CLOSING,		// 9
	TCP_TIME_WAIT,		// 10
};

#define READABLE(serv) \
	((serv->rcvring.count > 0) \
	|| (serv->state == TCP_ESTABLISHED) \
	|| (serv->state == TCP_FIN_WAIT_1) \
	|| (serv->state == TCP_FIN_WAIT_2))

#define WRITABLE(serv) \
	((serv->state == TCP_ESTABLISHED) \
		|| (serv->state == TCP_CLOSE_WAIT) \
		|| (serv->state == TCP_SYN))

#define TCPSND_BUFFSIZE PAGE_SIZE
#define TCPRCV_BUFFSIZE PAGE_SIZE

#define XMT_MSS 1460
#define RCV_MSS 536

#define BYTE_ACK 0
#define BYTE_RST 1
#define BYTE_OTH 2

#define XMTFIN(serv)		(serv->xmtfin == 1) // to send fin
#define XMTFIN_NOACK(serv)	(serv->xmtfin == 2) // sent fin, but not acked
#define XMTFIN_ACKED(serv)	(serv->xmtfin == 3) // fin has being acked
#define XMTFIN_SENT(serv)	(serv->xmtfin >= 2) // fin has been sent
#define SERVCLOSED(serv)	(serv->xmtfin > 0)  // user has called close

#define RCVFIN(serv)		(serv->rcvfin == 1) // just get fin
#define RCVFIN_ACKED(serv)	(serv->rcvfin == 2) // has acked fin

#define NOTIMER(serv) 		(serv->timertype == 0)
#define DELAY(serv)		(serv->timertype == 1)
#define REXMT(serv)		(serv->timertype == 2)
#define PERSISTENT(serv)	(serv->timertype == 3)
#define KEEPALIVE(serv)		(serv->timertype == 4)
#define ANYTIMER(serv)		(serv->timertype > 0)

typedef struct tcpserv{
	list_t list;

	struct sockaddr_in srcaddr, dstaddr;

	short state;
	char xmtfin; // user has closed socket, send fin, and didn't get ack yet
	char rcvfin; // has gotten fin sent by the other side

	ring_buffer_t xmtring;
	ring_buffer_t rcvring;

	list_t rcvskblist;

	int timertype;
	int timercount; // timer count for nothing to do
	synctimer_t delaytimer;

	int rexmttimes;

	int keepalive_count;

	struct wait_queue * xmtwait, * rcvwait;
	struct socket * socket;
	struct _netif * netif;

	// xmtseq: next byte will be sent by self,
	// xmtack: byte next to be confirmed by the other end
	unsigned long xmtseq, xmtack; 

	// rcvseq: next byte will receive, sent by the other end
	// rcvack: byte next to be confirmed, sent by the other end
	unsigned long rcvseq, rcvack;

	char xmtwin_update;

	unsigned short xmtwin, rcvwin;
	unsigned short xmtmss, rcvmss;
	int synnum, backlog;

	char rcvurgs;		// rcv urg state
	unsigned char rcvurgc;	// rcv urg char

	BOOLEAN bxmturg;
	unsigned long xmturgseq; // xmt urg seq

	int tcp_nodelay;
	int tcp_stdurg;	
}tcpserv_t;

#define DEFAULT_RING_SIZE (PAGE_SIZE * 2)

typedef struct sockopt{
	u32_t so_broadcast:1;
	u32_t so_debug:1;
	u32_t so_dontroute:1;
	u32_t so_keepalive;
	u32_t so_oobinline:1;
	u32_t so_reuseaddr:1;
	u32_t so_reuseport:1;
	int so_error;
	struct linger so_linger;
	int so_rcvbuf, so_sndbuf;
	int so_rcvlowat, so_sndlowat;
	struct timeval so_rcvtimeo, so_sndtimeo;
}sockopt_t;

#define SOCK_INIT 0

typedef struct socket{
	list_t list;
	unsigned short family;
	int type;
	int protocol;
	int state;
	sockopt_t opt;
	struct inode * inode;

	union{
		udpserv_t * udpserv;
		tcpserv_t * tcpserv;
		void * serv;
	}u;

	select_t select;
}socket_t;

#define userv u.udpserv
#define tserv u.tcpserv

extern BOOLEAN sock_can_read(socket_t * sock);
extern BOOLEAN sock_can_write(socket_t * sock);
extern BOOLEAN sock_can_except(socket_t * sock);

#endif
