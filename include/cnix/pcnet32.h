#ifndef PCNET32_H
#define PCNET32_H
#include <cnix/ether.h>

#define LOG_NR_DRES	(2)
#define NR_DRES		(1 << LOG_NR_DRES)
#define RTX_BUFF_SIZE		(1544)
#define MAX_ETHER_DEVICE	2

typedef struct _rcv_desc_ring_entry
{
	char *rcv_buffer;	// receive buffer's address
	u16_t rcv_buffer_len;	// complement
	u16_t rcv_flags;
	u16_t rcv_msg_len;	// the acutal data length
	u16_t rcv_frame_tag;
	char *user_space;
} rcv_desc_ring_entry;

typedef struct _xmt_desc_ring_entry
{
	char	*xmt_buffer;		// buffer
	u16_t	xmt_len;		// complement
	u16_t	xmt_flags;
	u32_t	xmt_reserved;
	char	*user_space;
} xmt_desc_ring_entry;

typedef struct _rtx_ring_buffer
{
	rcv_desc_ring_entry rcv_ring[NR_DRES];
	xmt_desc_ring_entry xmt_ring[NR_DRES];

	int rcv_index;
	int xmt_head_index;
	int xmt_tail_index;
} rtx_ring_buffer_t;


typedef struct _ether_pcnet
{
	LOCAL_NETIF

	// Same as ether_netif_t
	unsigned long io_base_addr;
	int irq_no;
	BOOLEAN (*transmit)(struct _ether_pcnet *netif);
	int (*ioctl)(netif_t *netif, int flag);
	
	//
	rtx_ring_buffer_t rtx_ring_buffer;
} ether_pcnet_t;

#endif
