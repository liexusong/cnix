#ifndef _TCP_H
#define _TCP_H

#include <cnix/types.h>
#include <cnix/socket.h>

typedef struct psetcphdr{
	unsigned long srcip;
	unsigned long dstip;
	unsigned char res;
	unsigned char protocol;
	unsigned short len;
	tcphdr_t tcp;
}psetcphdr_t;

typedef struct fsrpau{
	u16_t fin:1;
	u16_t syn:1;
	u16_t rst:1;
	u16_t psh:1;
	u16_t ack:1;
	u16_t urg:1;
	u16_t res2:2;
}fsrpau_t;

typedef struct tcp_param{
	int seq;
	int ack_seq;
	fsrpau_t bits;
	int window;
	int mss; // option
}tcp_param_t;

extern void delay_timer_callback(void * data);
extern void start_delay_timer(tcpserv_t * serv);
extern void restart_delay_timer(tcpserv_t * serv);
extern void stop_delay_timer(tcpserv_t * serv);
extern void start_time_wait_timer(tcpserv_t * serv);

extern int cs_tcp_packet(
	tcpserv_t * serv,
	tcp_param_t * param,
	ring_buffer_t * ring,	// data to send
	int off,
	int data_len,		// length to copy
	int wait
	);
extern tcpserv_t * get_match_tcpserv(iphdr_t * ip, tcphdr_t * tcp);
extern void send_tcp_rst(tcphdr_t * tcp, iphdr_t * ip);
extern BOOLEAN insert_tcp_packet(
	tcpserv_t * serv, skbuff_t * skb, tcphdr_t * tcp, BOOLEAN * fin
	);
extern void free_all_resource(tcpserv_t * serv);
extern tcpserv_t * alloc_tcpserv(BOOLEAN need);
extern void add_tcpserv(tcpserv_t * serv);
extern void tcp_free_port(tcpserv_t * serv);
extern void free_tcpserv(tcpserv_t * serv);
extern void wakeup_for_tcpsel(tcpserv_t * serv, short optype);

#endif
