#ifndef _SKBUFF_H
#define _SKBUFF_H

#include <cnix/types.h>
#include <cnix/list.h>

#define __inline__ inline

// type
#define SK_NAME	1		// socket name
#define SK_DATA	2		// socket data

// flag
#define	SK_EXT_MEM	1	// use extent memory
#define SK_PK_HEAD_DATA	2	// the protocol head and data are in the packet
#define SK_PK_HEAD	4	// only the protocol head in this packet
#define SK_PK_DATA	8	// only protocol data


#define SKBUFFSIZE	128
#define SKEXTBUFFSIZE	2048
#define MAX_PACKET_SIZE 2048
struct _netif;

typedef struct skbuff
{
	list_t pkt_list;	// next ip packet
	list_t list;		// next skbuff in one ip packet
		
	struct _netif * netif;
	int flag;		// buffer flags
	int type;		// buffer type

	int dgram_len;		// the total length of packet
	int dlen;		// length of the buffer
	int data_ptr;		// point to the valid data
	unsigned char * data;	// data buffer
	unsigned char * ext_data;	// extent buffer
}skbuff_t;

static  __inline__ unsigned char * sk_get_buff_ptr(skbuff_t *sk)
{
	if (!sk)
	{
		return NULL;
	}
	
	if (sk->flag & SK_EXT_MEM)
	{
		return sk->ext_data;
	}

	return sk->data;
}

extern int skb_free_nr;

// free the node for root
#define plant_sk_buff(root, node)		\
	do {				\
		node->nextpkt = root;	\
		root = node;		\
		node = NULL;		\
	} while (0)

// allocate one node from root
#define uproot_sk_buff(root, node)	\
	do {					\
		node = root;			\
		if (root)			\
			root = root->nextpkt;	\
	} while (0)
			
		
		
#define MAX_IPBUFF_NUM 1024

extern skbuff_t * skb_alloc(int size, int wait);
extern void skb_free(skbuff_t * skb);
extern void skb_free_packet(skbuff_t *skb);
extern skbuff_t * skb_move_up(skbuff_t * skb, int len);
#endif
