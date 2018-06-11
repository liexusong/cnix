#ifndef ROUTE_H
#define ROUTE_H

#include <cnix/types.h>
#include <cnix/netif.h>

typedef struct _route
{
	list_t	list;		// double link list
	
	ip_addr_t dest;		// destination network or host
	ip_addr_t gateway;	// gateway address
	ip_addr_t netmask;	// netmask for the destination network.
	int	flag;		//
	
	int	metric;		// not used
	int	ref;		// not used
	int	use;		// not used
	
	netif_t *netif;
} route_t;

/* This structure gets passed by the SIOCADDRT and SIOCDELRT calls. */
struct rtentry
{
	unsigned long int rt_pad1;
	struct sockaddr rt_dst;		/* Target address.  */
	struct sockaddr rt_gateway;		/* Gateway addr (RTF_GATEWAY).  */
	struct sockaddr rt_genmask;		/* Target network mask (IP).  */
	unsigned short int rt_flags;
	short int rt_pad2;
	unsigned long int rt_pad3;
	unsigned char rt_tos;
	unsigned char rt_class;
	short int rt_pad4;
	short int rt_metric;		/* +1 for binary compatibility!  */
	char *rt_dev;			/* Forcing the device at add.  */
	unsigned long int rt_mtu;		/* Per route MTU/Window.  */
	unsigned long int rt_window;	/* Window clamping.  */
	unsigned short int rt_irtt;		/* Initial RTT.  */
};
/* Compatibility hack.  */
#define rt_mss	rt_mtu

#define	RTF_UP		0x0001		/* Route usable.  */
#define	RTF_GATEWAY	0x0002		/* Destination is a gateway.  */

#define	RTF_HOST	0x0004		/* Host entry (net otherwise).  */
#define RTF_REINSTATE	0x0008		/* Reinstate route after timeout.  */
#define	RTF_DYNAMIC	0x0010		/* Created dyn. (by redirect).  */
#define	RTF_MODIFIED	0x0020		/* Modified dyn. (by redirect).  */
#define RTF_MTU		0x0040		/* Specific MTU for this route.  */
#define RTF_MSS		RTF_MTU		/* Compatibility.  */
#define RTF_WINDOW	0x0080		/* Per route window clamping.  */
#define RTF_IRTT	0x0100		/* Initial round trip time.  */
#define RTF_REJECT	0x0200		/* Reject route.  */
#define	RTF_STATIC	0x0400		/* Manually injected route.  */
#define	RTF_XRESOLVE	0x0800		/* External resolver.  */
#define RTF_NOFORWARD   0x1000		/* Forwarding inhibited.  */
#define RTF_THROW	0x2000		/* Go to next class.  */
#define RTF_NOPMTUDISC  0x4000		/* Do not send packets with DF.  */

/* Bad idea. IPv6 should not use broken IPv4 interface.  */

#define RTF_ADDRCONF	0x0800		/* Announced on link prefix.  */
#define RTF_INVALID	0x1000
#define RTF_DCACHE	0x2000
#define RTF_DEFAULT	0x4000		/* Route is a default route.  */
#define RTF_NEXTHOP	0x8000		/* Non gateway route with nexthop.  */


#define RTF_MAGIC	0x10000		/* Route added/deleted automatically,
					   when interface changes its state. */

#define RTCF_VALVE	0x00200000
#define RTCF_MASQ	0x00400000
#define RTCF_NAT	0x00800000
#define RTCF_DOREDIRECT 0x01000000
#define RTCF_LOG	0x02000000
#define RTCF_DIRECTSRC	0x04000000

#define RTF_LOCAL	0x80000000
#define RTF_INTERFACE	0x40000000
#define RTF_MULTICAST	0x20000000
#define RTF_BROADCAST	0x10000000
#define RTF_NAT		0x08000000

#define RTF_ADDRCLASSMASK	0xF8000000
#define RT_ADDRCLASS(flags)	((u32_t) flags >> 23)

#define RT_TOS(tos)		((tos) & IPTOS_TOS_MASK)

#define RT_LOCALADDR(flags)	((flags & RTF_ADDRCLASSMASK) \
				 == (RTF_LOCAL|RTF_INTERFACE))

#define RT_CLASS_UNSPEC		0
#define RT_CLASS_DEFAULT	253

#define RT_CLASS_MAIN		254
#define RT_CLASS_LOCAL		255
#define RT_CLASS_MAX		255


#define RTMSG_ACK		NLMSG_ACK
#define RTMSG_OVERRUN		NLMSG_OVERRUN

#define RTMSG_NEWDEVICE		0x11
#define RTMSG_DELDEVICE		0x12
#define RTMSG_NEWROUTE		0x21
#define RTMSG_DELROUTE		0x22
#define RTMSG_NEWRULE		0x31
#define RTMSG_DELRULE		0x32
#define RTMSG_CONTROL		0x40

#define RTMSG_AR_FAILED		0x51	/* Address Resolution failed.  */


extern int do_with_route_entry(struct rtentry *ent, int op);

#endif
