#ifndef _IP_H
#define _IP_H

#include <cnix/types.h>
#include <cnix/arp.h>
#include <cnix/ether.h>

#define IPV4	4
#define IPV6	6

// more ip group
#define IP_MORE	(1 << 13)
// don't fragment
#define IP_DF	(1 << 14)
//
#define FRAG_MASK ((1 << 13) - 1)

#define IP_NORM_HEAD_LEN	sizeof(iphdr_t)

//#define LINK_HEAD_LEN	(6 + 6 + 2)
//#define LINK_LEN	(6 + 6 + 2)

#define SK_TO_DATA(ip, skb, type)		\
	do {					\
		unsigned char *__p;		\
		__p = sk_get_buff_ptr(skb);	\
		__p += skb->data_ptr;		\
		ip = (type *)__p;		\
	} while (0)

#define SK_TO_IP_HDR(p, skb) SK_TO_DATA(p, skb, iphdr_t)
#define SK_TO_ICMP_HDR(p, skb) SK_TO_DATA(p, skb, icmphdr_t)
#define SK_TO_UDP_HDR(p, skb) SK_TO_DATA(p, skb, udphdr_t)
#define SK_TO_TCP_HDR(p, skb) SK_TO_DATA(p, skb, tcphdr_t)

/* the byte order of u16_t & u32_t is big endia */
typedef struct iphdr
{
	u8_t ihl:4;
	u8_t version:4;
	u8_t tos;
	u16_t tot_len;
	u16_t id;
	u16_t frag_off;
	u8_t ttl;
	u8_t protocol;
	u16_t check;
	u32_t saddr;
	u32_t daddr;
}iphdr_t;

enum
{
	IPPROTO_IP = 0,
	IPPROTO_ICMP = 1,
	IPPROTO_TCP = 6,
	IPPROTO_UDP = 17
};

typedef struct tcphdr{
	u16_t source;
	u16_t dest;
	u32_t seq;
	u32_t ack_seq;
	u16_t res1:4;
	u16_t doff:4;
	u16_t fin:1;
	u16_t syn:1;
	u16_t rst:1;
	u16_t psh:1;
	u16_t ack:1;
	u16_t urg:1;
	u16_t res2:2;
	u16_t window;
	u16_t check;
	 u16_t urg_ptr;
}tcphdr_t;

#define TCPOPT_END 0 // end of option list
#define TCPOPT_NOP 1 // no operation
#define TCPOPT_MSS 2 // maximum segment size
#define TCPOPT_WIN 3 // window scale factor
#define TCPOPT_TSM 8 // time stamp option

typedef struct udphdr{
	u16_t source;
	u16_t dest;
	u16_t len;
	u16_t check;
}udphdr_t;

typedef struct icmphdr{
	u8_t type;
	u8_t code;
	u16_t checksum;

	union{
		struct
		{
			u16_t id;
			u16_t sequence;
		} echo;
		
		u32_t gateway;

		struct{
			u16_t __unused;
			u16_t mtu;
		}frag;
	} un;
}icmphdr_t;

/* icmphdr type */
#define ICMP_ECHOREPLY		0
#define ICMP_DEST_UNREACH	3
#define ICMP_SOURCE_QUENCH	4
#define ICMP_REDIRECT		5
#define ICMP_ECHO		8
#define ICMP_TIME_EXCEEDED	11
#define ICMP_PARAMETERPROB	12
#define ICMP_TIMESTAMP		13
#define ICMP_TIMESTAMPREPLY	14
#define ICMP_INFO_REQUEST	15
#define ICMP_INFO_REPLY		16
#define ICMP_ADDRESS		17
#define ICMP_ADDRESSREPLY	18
#define NR_ICMP_TYPES		18

/* icmphdr code */
#define ICMP_NET_UNREACH	0
#define ICMP_HOST_UNREACH	1
#define ICMP_PROT_UNREACH	2
#define ICMP_PORT_UNREACH	3
#define ICMP_FRAG_NEEDED	4
#define ICMP_SR_FAILED		5
#define ICMP_NET_UNKNOWN	6
#define ICMP_HOST_UNKNOWN	7
#define ICMP_HOST_ISOLATED	8
#define ICMP_NET_ANO		9
#define ICMP_HOST_ANO		10
#define ICMP_NET_UNR_TOS	11
#define ICMP_HOST_UNR_TOS	12
#define ICMP_PKT_FILTERED	13
#define ICMP_PREC_VIOLATION	14
#define ICMP_PREC_CUTOFF	15
#define NR_ICMP_UNREACH		15

/* icmphdr code for REDIRECT */
#define ICMP_REDIR_NET		0
#define ICMP_REDIR_HOST		1
#define ICMP_REDIR_NETTOS	2
#define ICMP_REDIR_HOSTTOS	3

/* icmphdr code for TIME_EXCEEDED */
#define ICMP_EXC_TTL		0
#define ICMP_EXC_FRAGTIME	1

extern void input_ip_packet(skbuff_t *skb);
extern void input_raw_packet(skbuff_t *skb, iphdr_t * ip);
extern void input_icmp_packet(skbuff_t *skb, iphdr_t * ip);
extern void input_udp_packet(skbuff_t *skb, iphdr_t * ip);
extern void input_tcp_packet(skbuff_t *skb, iphdr_t * ip);

extern int output_ip_packet(skbuff_t *skb, ip_addr_t *ip_addr, int proto);
extern BOOLEAN output_netif(
	skbuff_t *skb, ip_addr_t *ip_addr, ether_netif_t * eth
	);

extern BOOLEAN add_reply_list(skbuff_t * skb, ip_addr_t * ip_addr, int proto);

#endif
