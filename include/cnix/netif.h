#ifndef NETIF_H
#define NETIF_H

#include <cnix/types.h>
#include <cnix/list.h>
#include <cnix/skbuff.h>
#include <cnix/socket.h>

struct _netif;

#define IP_ADDR_LEN	4
#define MAX_DEVICE_NAME		16
#define IFNAMSIZ		MAX_DEVICE_NAME

// flag value
#define NETIF_UP	IFF_UP
#define NETIF_LOOPBACK	IFF_LOOPBACK
#define NETIF_RUNNING	IFF_RUNNING
#define NETIF_MULTICAST	IFF_MULTICAST
#define NETIF_BROADCAST	IFF_BROADCAST
/* Standard interface flags. */
enum
  {
    IFF_UP = 0x1,		/* Interface is up.  */
# define IFF_UP	IFF_UP
    IFF_BROADCAST = 0x2,	/* Broadcast address valid.  */
# define IFF_BROADCAST	IFF_BROADCAST
    IFF_DEBUG = 0x4,		/* Turn on debugging.  */
# define IFF_DEBUG	IFF_DEBUG
    IFF_LOOPBACK = 0x8,		/* Is a loopback net.  */
# define IFF_LOOPBACK	IFF_LOOPBACK
    IFF_POINTOPOINT = 0x10,	/* Interface is point-to-point link.  */
# define IFF_POINTOPOINT IFF_POINTOPOINT
    IFF_NOTRAILERS = 0x20,	/* Avoid use of trailers.  */
# define IFF_NOTRAILERS	IFF_NOTRAILERS
    IFF_RUNNING = 0x40,		/* Resources allocated.  */
# define IFF_RUNNING	IFF_RUNNING
    IFF_NOARP = 0x80,		/* No address resolution protocol.  */
# define IFF_NOARP	IFF_NOARP
    IFF_PROMISC = 0x100,	/* Receive all packets.  */
# define IFF_PROMISC	IFF_PROMISC

    /* Not supported */
    IFF_ALLMULTI = 0x200,	/* Receive all multicast packets.  */
# define IFF_ALLMULTI	IFF_ALLMULTI

    IFF_MASTER = 0x400,		/* Master of a load balancer.  */
# define IFF_MASTER	IFF_MASTER
    IFF_SLAVE = 0x800,		/* Slave of a load balancer.  */
# define IFF_SLAVE	IFF_SLAVE

    IFF_MULTICAST = 0x1000,	/* Supports multicast.  */
# define IFF_MULTICAST	IFF_MULTICAST

    IFF_PORTSEL = 0x2000,	/* Can set media type.  */
# define IFF_PORTSEL	IFF_PORTSEL
    IFF_AUTOMEDIA = 0x4000	/* Auto media select active.  */
# define IFF_AUTOMEDIA	IFF_AUTOMEDIA
  };

// generic network interface function.

typedef struct _ip_addr
{
	u8_t	ip[IP_ADDR_LEN];
} ip_addr_t;


#define LOCAL_NETIF_ADDR	\
	list_t list;		\
	struct _netif *netif;	\
	ip_addr_t ip_addr;	\
	ip_addr_t netmask;	\
	ip_addr_t bcast_addr;	

#define LOCAL_NETIF				\
	list_t list;				\
	char netif_name[MAX_DEVICE_NAME];	\
	unsigned long rx_frame_count;		\
	unsigned long tx_frame_count;		\
	unsigned long rx_error_count;		\
	unsigned long tx_error_count;		\
	unsigned long rx_byte_count;		\
	unsigned long tx_byte_count;		\
	list_t in_queue;			\
	list_t out_queue;			\
	int netif_mtu;				\
	int metric;				\
	int flag;				\
	BOOLEAN (*netif_output)(struct _netif *netif, 	\
				skbuff_t *skb, 		\
				ip_addr_t *ip_addr,	\
				ip_addr_t *gw_ipaddr);	\
	void (*netif_input)(struct _netif *netif);	\
	int (*netif_ioctl)(struct _netif *netif, 	\
				int request,		\
				void * data,		\
				int openflags);		\
	netif_addr_t *netif_addr;	

// base class
typedef struct _netif_addr
{
	LOCAL_NETIF_ADDR
} netif_addr_t;

typedef struct _netif 
{
	LOCAL_NETIF
} netif_t;

// data struct from the user
/*
 * Interface request structure used for socket
 * ioctl's.  All interface ioctl's must have parameter
 * definitions which begin with ifr_name.  The
 * remainder may be interface specific.
 */
struct	ifreq {
	char	ifr_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	union {
		struct	sockaddr ifru_addr;
		struct	sockaddr ifru_dstaddr;
		struct	sockaddr ifru_broadaddr;
		struct	sockaddr ifru_netmask;
		short	ifru_flags[2];
		short	ifru_index;
		int	ifru_metric;
		int	ifru_mtu;
		int	ifru_phys;
		int	ifru_media;
		caddr_t	ifru_data;
		int	ifru_cap[2];
	} ifr_ifru;
#define	ifr_addr	ifr_ifru.ifru_addr	/* address */
#define	ifr_dstaddr	ifr_ifru.ifru_dstaddr	/* other end of p-to-p link */
#define	ifr_broadaddr	ifr_ifru.ifru_broadaddr	/* broadcast address */
#define	ifr_netmask	ifr_ifru.ifru_netmask	/* interface net mask	*/
#define	ifr_flags	ifr_ifru.ifru_flags[0]	/* flags */
#define	ifr_prevflags	ifr_ifru.ifru_flags[1]	/* flags */
#define	ifr_metric	ifr_ifru.ifru_metric	/* metric */
#define	ifr_mtu		ifr_ifru.ifru_mtu	/* mtu */
#define ifr_phys	ifr_ifru.ifru_phys	/* physical wire */
#define ifr_media	ifr_ifru.ifru_media	/* physical media */
#define	ifr_data	ifr_ifru.ifru_data	/* for use by interface */
#define	ifr_reqcap	ifr_ifru.ifru_cap[0]	/* requested capabilities */
#define	ifr_curcap	ifr_ifru.ifru_cap[1]	/* current capabilities */
#define	ifr_index	ifr_ifru.ifru_index	/* interface index */
};

/*
 * Structure used in SIOCGIFCONF request.
 * Used to retrieve interface configuration
 * for machine (useful for programs which
 * must know all networks accessible).
 */
struct	ifconf {
	int	ifc_len;		/* size of associated buffer */
	union {
		caddr_t	ifcu_buf;
		struct	ifreq *ifcu_req;
	} ifc_ifcu;
#define	ifc_buf	ifc_ifcu.ifcu_buf	/* buffer address */
#define	ifc_req	ifc_ifcu.ifcu_req	/* array of structures returned */
};

typedef struct ifconf ifconf_t;
typedef struct ifreq ifreq_t;

#define CHECK_PERM()	\
	do {		\
		if (current->euid != 0 && current->uid != 0)	\
		{						\
			return -EPERM;				\
		}						\
	} while (0)


extern BOOLEAN add_netif(netif_t *net);
extern BOOLEAN del_netif(netif_t *net);
extern netif_t * find_outif(ip_addr_t * ip, ip_addr_t * gw_ipaddr);
extern netif_t *find_netif_by_name(const char *name);
extern void netif_initialize(netif_t *netif);
extern void process_all_input(void);
extern BOOLEAN get_netif_ip_addr(netif_t *netif, ip_addr_t *pip);
extern BOOLEAN get_netif_netmask(netif_t *netif, ip_addr_t *mask);
extern BOOLEAN match_ifaddr(ip_addr_t * ip);
netif_t * get_outif(ip_addr_t * ip);

#endif
