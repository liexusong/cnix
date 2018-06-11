#ifndef _NET_H
#define _NET_H

#include <cnix/types.h>
#include <cnix/skbuff.h>
#include <cnix/ether.h>
#include <cnix/arp.h>
#include <cnix/socket.h>
#include <cnix/ip.h>
#include <cnix/route.h>

#define TCP_TEST 0

#define htons ntohs
#define htonl ntohl

#define iptoul(x) (*(unsigned long *)(x.ip))
#define ipptoul(x) (*(unsigned long *)(x->ip))
#define ultoip(x) (*(ip_addr_t *)&x)

extern unsigned short ntohs(u16_t val);
extern unsigned long ntohl(u32_t val);
extern unsigned short checksum(skbuff_t * skb);
extern unsigned short ipchecksum(skbuff_t * skb, int len);
extern socket_t * sock_alloc(int, int, int);
extern void sock_free(socket_t *);

#define UDPP 0
#define TCPP 1

extern int inet_port_alloc(int type);
extern void inet_port_free(int type, unsigned short port);
extern int inet_port_use(int type, unsigned short port);
BOOLEAN inet_port_busy(int type, unsigned short port);
extern void loopback_init(void);

extern void print_buff_ip_addr(char *buff, ip_addr_t *pip);

// get gateway from route table.
extern netif_t *get_netif(const ip_addr_t *dest, ip_addr_t *gw);
extern BOOLEAN add_route(route_t *r);
extern void netmask_ip(const ip_addr_t *ip_addr, 
		const ip_addr_t *mask, 
		ip_addr_t *ip_ret);
extern void init_route(void);

extern int udp_read(
	socket_t * sock,
	struct sockaddr_in * addr,
	int * addr_len,
	char * buffer,
	size_t count,
	int flags
	);

extern int udp_write(
	socket_t * sock,
	struct sockaddr_in * addr,
	const char * buffer,
	size_t count,
	int flags
	);

extern int tcp_read(
	socket_t * sock,
	char * buffer,
	size_t count,
	int flags
	);

extern int tcp_write(
	socket_t * sock,
	const char * buffer,
	size_t count,
	int flags
	);

extern int raw_read(
	socket_t * sock,
	struct sockaddr_in * addr,
	int * addr_len,
	char * buffer,
	size_t count,
	int flags
	);

extern int raw_write(
	socket_t * sock,
	struct sockaddr_in * addr,
	const char * buffer,
	size_t count,
	int flags
	);

extern void init_ip_hash_table(void);
extern void add_ip_hash_entry(skbuff_t *skb);
extern skbuff_t *get_ip_hash_entry(skbuff_t *skb);

extern BOOLEAN ip_eq(const ip_addr_t *ip1, const ip_addr_t *ip2);
extern BOOLEAN mac_addr_equal(const mac_addr_t *src, const mac_addr_t *dst);

extern void print_mac_addr(mac_addr_t *pmac);
extern void print_ip_addr(ip_addr_t *pip);
extern void print_packets(skbuff_t *head);
extern void print_packet(skbuff_t *head, int level);
extern int get_queue_count(list_t *head);

extern int udp_connect(socket_t * sock, struct sockaddr_in * addr);
extern int udp_bind(socket_t * sock, struct sockaddr_in * addr);
extern int udp_close(socket_t * sock);

extern int tcp_close(socket_t * sock);
extern int tcp_connect(socket_t * sock, struct sockaddr_in * addr);
extern int tcp_bind(socket_t * sock, struct sockaddr_in * addr);
extern int tcp_listen(socket_t * sock, int backlog);
extern int tcp_accept(
	socket_t * sock, struct sockaddr_in * addr, int * addr_len
	);
extern int tcp_shutdown(socket_t * sock, int how);

extern int raw_open(socket_t * sock);
extern int raw_close(socket_t * sock);

extern int tcp_sendmsg(
	socket_t * sock, const struct msghdr * message, int flags
	);
extern int udp_sendmsg(
	socket_t * sock, const struct msghdr * message, int flags
	);
extern int raw_sendmsg(
	socket_t * sock, const struct msghdr * message, int flags
	);

extern int tcp_recvmsg(
	socket_t * sock, struct msghdr * message, int flags
	);
extern int udp_recvmsg(
	socket_t * sock, struct msghdr * message, int flags
	);
extern int raw_recvmsg(
	socket_t * sock, struct msghdr * message, int flags
	);

extern int copy_from_iov(unsigned char * buffer, struct iovbuf * iovb, int len);
extern int copy_to_iov(unsigned char * buffer, struct iovbuf * iovb, int len);
extern int compare_seq(unsigned long seq, unsigned long seq2);

extern BOOLEAN raw_can_read(socket_t * sock);
extern BOOLEAN raw_can_write(socket_t * sock);
extern BOOLEAN udp_can_read(socket_t * sock);
extern BOOLEAN udp_can_write(socket_t * sock);
extern BOOLEAN tcp_can_read(socket_t * sock);
extern BOOLEAN tcp_can_write(socket_t * sock);
extern BOOLEAN tcp_can_except(socket_t * sock);

#endif
