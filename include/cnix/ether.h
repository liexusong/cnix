#ifndef ETHER_H
#define ETHER_H
#include <cnix/netif.h>

#define ETHER_MTU	1500
#define ETHER_LNK_HEAD_LEN	sizeof(ether_lnk_t)
#define LINK_HEAD_LEN		ETHER_LNK_HEAD_LEN
#define MAC_ADDR_LEN	6

typedef struct _mac_address
{
	unsigned char mac_addr[MAC_ADDR_LEN];
} mac_addr_t;

typedef struct _ether_netif
{
	LOCAL_NETIF		// base class member

	unsigned long io_base_addr;
	int irq_no;
	
	BOOLEAN (*transmit)(struct _ether_netif *netif);
	int (*ioctl)(struct _ether_netif *netif, int flag);
} ether_netif_t;

typedef struct _ether_netif_addr
{
	LOCAL_NETIF_ADDR	// base class member

	mac_addr_t ether_addr;
	mac_addr_t ether_bcast_addr;
} ether_netif_addr_t;

typedef struct _ether_lnk
{
	mac_addr_t  mac_dst;
	mac_addr_t mac_src;
	u16_t	lnk_type;	// packet type
} ether_lnk_t;

extern ether_netif_t *get_netif_by_irq(int irq);
extern BOOLEAN get_ether_mac_addr(ether_netif_t *netif, mac_addr_t *pmac);
extern BOOLEAN get_ether_boardcast_addr(ether_netif_t *netif, mac_addr_t *pmac);
extern ether_netif_t *get_netif_by_mac(mac_addr_t *pmac);
extern void ether_init(ether_netif_t *netif, 
		unsigned long base, 
		int irq,
		mac_addr_t *pmac);
#endif
