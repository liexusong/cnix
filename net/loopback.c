#include <asm/io.h>
#include <asm/system.h>
#include <cnix/types.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/errno.h>
#include <cnix/mm.h>
#include <cnix/netif.h>
#include <cnix/net.h>
#include <cnix/sockios.h>

#define LOOP_NAME	"lo"
#define LOOP_MTU	(16 * 1024)

static netif_t net_loop_back;
static netif_addr_t lo_addr;

static BOOLEAN lo_output(
	netif_t *netif,
	skbuff_t *head,
	ip_addr_t *ip_addr,
	ip_addr_t * gw_ipaddr
	);
static void lo_input(netif_t *netif);
static int lo_ioctl(netif_t *netif, int request,
				void * data, 
				int openflags);

void loopback_init(void)
{
	netif_t *pnet;
	netif_addr_t *paddr;
	ip_addr_t ip = {{127, 0, 0, 1}};

	paddr = &lo_addr;
	pnet = &net_loop_back;

	// init the loopback address
	memset(paddr, 0, sizeof(netif_addr_t));
	list_init(&paddr->list);
	paddr->netif = pnet;
	// copy ip address
	memcpy(&paddr->ip_addr, &ip, sizeof(ip_addr_t));
	ip.ip[0] = 255;
	memcpy(&paddr->netmask, &ip, sizeof(ip_addr_t));
	
	// init the loopback device
	memset(pnet, 0, sizeof(netif_t));
	netif_initialize((netif_t *)pnet);
	
	strcpy(pnet->netif_name, LOOP_NAME);
	
	pnet->netif_mtu = LOOP_MTU;	// 16k
	pnet->metric = 1;
	pnet->flag = NETIF_UP | NETIF_LOOPBACK | NETIF_RUNNING;
	pnet->netif_addr = paddr;

	pnet->netif_input = lo_input;
	pnet->netif_output = lo_output;
	pnet->netif_ioctl = lo_ioctl;

	add_netif(&net_loop_back);
}

static BOOLEAN lo_output(
	netif_t *netif, skbuff_t *head, ip_addr_t *ip_addr, ip_addr_t *gw_ipaddr
	)
{
	unsigned long flags;
	
	lockb_all(flags);
	list_add_tail(&netif->in_queue, &head->pkt_list);	//XXX
	unlockb_all(flags);
	
	raise_bottom(PCNET32_B);
	
	return TRUE;
}

static void lo_input(netif_t *netif)
{
	unsigned long flags;
	skbuff_t * skb;
	list_t *node;
	
	lock(flags);
	while (!list_empty(&netif->in_queue))
	{
		// get one packet
		node = netif->in_queue.next;
		list_del1(node);
		unlock(flags);

		skb = list_entry(node, pkt_list, skbuff_t);
		netif->rx_frame_count++;
		netif->rx_byte_count += skb->dgram_len;

		skb->netif = find_netif_by_name(LOOP_NAME);
		if (!skb->netif)
		{
			PANIC("no loopback!\n");
		}

		//print_packet(skb, 1);
		
		input_ip_packet(skb);
		
		lock(flags);
	}
	unlock(flags);
}


static int lo_ioctl(netif_t *netif, int request,
				void *data,
				int openflags)
{
	int error = 0;
	ifreq_t *req = (ifreq_t *)data;
	
	switch (request)
	{
		case SIOCGIFMTU:
		{
			req->ifr_mtu = netif->netif_mtu;
			break;
		}
			
		case SIOCSIFMTU:
		{
			CHECK_PERM();
			if (req->ifr_mtu > LOOP_MTU)
			{
				error = -EINVAL;
			}
			else
			{
				netif->netif_mtu = req->ifr_mtu;
			}

			break;
		}

		case SIOCGIFFLAGS:
		{
			req->ifr_flags = netif->flag;
			
			break;
		}

		case SIOCSIFFLAGS:
		{
			CHECK_PERM();
			

			break;
		}

		case SIOCGIFHWADDR:
		{
			struct sockaddr *sock;
			
			sock = &req->ifr_addr;
			sock->sa_family = ARPHRD_LOOPBACK;
			memset(&sock->sa_data, 0, 14);

			break;
		}

		default:
			error = -ENOTSUP;
			break;
	}

	return error;
}
