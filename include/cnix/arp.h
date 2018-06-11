#ifndef ARP_H
#define ARP_H

#include <cnix/skbuff.h>
#include <cnix/ether.h>

#define MAX_ARP_ENTRY	16
#define INIT_ARP_TIME	20
#define MAX_ARP_REQ	5

#define ARP_TIMEOUT	10
#define MS2HZ(ms)	(ms / 10)

/* ARP protocol HARDWARE identifiers. */
#define ARPHRD_NETROM	0		/* From KA9Q: NET/ROM pseudo. */
#define ARPHRD_ETHER 	1		/* Ethernet 10/100Mbps.  */
#define	ARPHRD_EETHER	2		/* Experimental Ethernet.  */
#define	ARPHRD_AX25	3		/* AX.25 Level 2.  */
#define	ARPHRD_PRONET	4		/* PROnet token ring.  */
#define	ARPHRD_CHAOS	5		/* Chaosnet.  */
#define	ARPHRD_IEEE802	6		/* IEEE 802.2 Ethernet/TR/TB.  */
#define	ARPHRD_ARCNET	7		/* ARCnet.  */
#define	ARPHRD_APPLETLK	8		/* APPLEtalk.  */
#define	ARPHRD_DLCI	15		/* Frame Relay DLCI.  */
#define	ARPHRD_ATM	19		/* ATM.  */
#define	ARPHRD_METRICOM	23		/* Metricom STRIP (new IANA id).  */
#define ARPHRD_IEEE1394	24		/* IEEE 1394 IPv4 - RFC 2734.  */
#define ARPHRD_EUI64		27		/* EUI-64.  */
#define ARPHRD_INFINIBAND	32		/* InfiniBand.  */

/* Dummy types for non ARP hardware */
#define ARPHRD_SLIP	256
#define ARPHRD_CSLIP	257
#define ARPHRD_SLIP6	258
#define ARPHRD_CSLIP6	259
#define ARPHRD_RSRVD	260		/* Notional KISS type.  */
#define ARPHRD_ADAPT	264
#define ARPHRD_ROSE	270
#define ARPHRD_X25	271		/* CCITT X.25.  */
#define ARPHDR_HWX25	272		/* Boards with X.25 in firmware.  */
#define ARPHRD_PPP	512
#define ARPHRD_CISCO	513		/* Cisco HDLC.  */
#define ARPHRD_HDLC	ARPHRD_CISCO
#define ARPHRD_LAPB	516		/* LAPB.  */
#define ARPHRD_DDCMP	517		/* Digital's DDCMP.  */
#define	ARPHRD_RAWHDLC	518		/* Raw HDLC.  */

#define ARPHRD_TUNNEL	768		/* IPIP tunnel.  */
#define ARPHRD_TUNNEL6	769		/* IPIP6 tunnel.  */
#define ARPHRD_FRAD	770             /* Frame Relay Access Device.  */
#define ARPHRD_SKIP	771		/* SKIP vif.  */
#define ARPHRD_LOOPBACK	772		/* Loopback device.  */
#define ARPHRD_LOCALTLK 773		/* Localtalk device.  */
#define ARPHRD_FDDI	774		/* Fiber Distributed Data Interface. */
#define ARPHRD_BIF	775             /* AP1000 BIF.  */
#define ARPHRD_SIT	776		/* sit0 device - IPv6-in-IPv4.  */
#define ARPHRD_IPDDP	777		/* IP-in-DDP tunnel.  */
#define ARPHRD_IPGRE	778		/* GRE over IP.  */
#define ARPHRD_PIMREG	779		/* PIMSM register interface.  */
#define ARPHRD_HIPPI	780		/* High Performance Parallel I'face. */
#define ARPHRD_ASH	781		/* (Nexus Electronics) Ash.  */
#define ARPHRD_ECONET	782		/* Acorn Econet.  */
#define ARPHRD_IRDA	783		/* Linux-IrDA.  */
#define ARPHRD_FCPP	784		/* Point to point fibrechanel.  */
#define ARPHRD_FCAL	785		/* Fibrechanel arbitrated loop.  */
#define ARPHRD_FCPL	786		/* Fibrechanel public loop.  */
#define ARPHRD_FCFABRIC 787		/* Fibrechanel fabric.  */
#define ARPHRD_IEEE802_TR 800		/* Magic type ident for TR.  */
#define ARPHRD_IEEE80211 801		/* IEEE 802.11.  */

enum _packet_type
{
	IP_TYPE  = 0x0800,
	ARP_TYPE = 0x0806,
	RARP_TYPE = 0x0835
};

enum hard_protocol_type
{
	H_ETHER_ADDR_TYPE	= 0x0001,
	P_IP_ADDR_TYPE		= 0x0800  
};

enum operate_type
{
	ARP_REQ	= 0x1,
	ARP_REPLY = 0x2,
	RARP_REQ = 0x4,
	RARP_REPLY = 0x8
};

typedef struct _arp_entry
{
	list_t list;
	int life_time;
	ip_addr_t ip_addr;
	mac_addr_t mac_addr;
} arp_entry_t;

typedef struct _arp_packet 
{
	mac_addr_t  mac_dst;
	mac_addr_t mac_src;
	u16_t	packet_type;	// packet type
	u16_t	h_type;		//hardware type
	u16_t	p_type;		// protocol type
	u8_t	h_len;		// hard address length
	u8_t	p_len;		// protocol address length
	u16_t	op_type;	// operate type
	mac_addr_t mac_sender;
	ip_addr_t  ip_sender;
	mac_addr_t mac_sendback;
	ip_addr_t  ip_sendback;
	u8_t	padding[18];
} arp_packet_t __attribute__ ((aligned(1)));

extern void init_arp_queue(void);
extern void input_arp_packet(skbuff_t *skb);
extern BOOLEAN get_mac_addr_by_arp(
	ether_netif_t *netif, ip_addr_t *ip, mac_addr_t * pentry
	);

#endif
