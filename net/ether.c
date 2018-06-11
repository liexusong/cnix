#include <asm/io.h>
#include <asm/system.h>
#include <cnix/types.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/errno.h>
#include <cnix/mm.h>
#include <cnix/driver.h>
#include <cnix/arp.h>
#include <cnix/ether.h>
#include <cnix/net.h>
#include <cnix/sockios.h>

#define sub_str_eq(x, y, n)	(strncmp(x, y, n) == 0)

static BOOLEAN ether_output(netif_t *netif,
				skbuff_t *skb, 
				ip_addr_t *ip_addr,
				ip_addr_t *gw_ipaddr);
static void ether_input(netif_t *netif);
static int ether_ioctl(netif_t *netif, int request,
				void * data, 
				int openflags);

static BOOLEAN get_dest_mac_addr(netif_t *netif0,
			const ip_addr_t *dest_ip, 
			mac_addr_t *pmac);
static int nr = 0;

extern list_t net_device_list;

void ether_init(ether_netif_t *netif, 
		unsigned long base, 
		int irq,
		mac_addr_t *pmac)
{
	ether_netif_addr_t *addr;
	ip_addr_t my_ip = {{192, 168, 23, 123}};
	ip_addr_t my_mask = {{255, 255, 255, 0}};
	ip_addr_t my_bcast = {{192, 168, 23, 255}};
	mac_addr_t my_eth_bcast = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
	
	netif_initialize((netif_t *)netif);
	
	sprintf(netif->netif_name, "eth%d", nr);
	nr++;
	netif->netif_mtu = ETHER_MTU;
	netif->metric = 1;
	netif->flag = NETIF_UP;

	netif->io_base_addr = base;
	netif->irq_no = irq;

	addr = (ether_netif_addr_t *)kmalloc(sizeof(ether_netif_addr_t), 0);
	if (!addr)
	{
		PANIC("can not allocate memory!\n");
	}

	// initialize the address
	list_init(&addr->list);
	addr->netif = (netif_t *)netif;
	
	// copy ip address
	memcpy(&addr->ip_addr, &my_ip, IP_ADDR_LEN);
	memcpy(&addr->netmask, &my_mask, IP_ADDR_LEN);
	memcpy(&addr->bcast_addr, &my_bcast, IP_ADDR_LEN);
	
	// copy mac address
	memcpy(&addr->ether_addr, pmac, sizeof(mac_addr_t));
	memcpy(&addr->ether_bcast_addr, &my_eth_bcast, sizeof(mac_addr_t));

	// initialize the netif
	netif->netif_addr = (netif_addr_t *)addr;
	//
	netif->netif_input = ether_input;
	netif->netif_output = ether_output;
	netif->netif_ioctl = ether_ioctl;

	add_netif((netif_t *)netif);
}

static BOOLEAN ether_output(netif_t *netif0,
				skbuff_t *skb, 
				ip_addr_t *ip_addr,
				ip_addr_t *gw_ipaddr)
{
	int space;
	skbuff_t *head;
	ether_lnk_t *plnk;
	unsigned char *p;
	mac_addr_t mac;
	ip_addr_t my_ip;
	BOOLEAN local;
	ether_netif_t *netif = (ether_netif_t *)netif0;

	space = skb->data_ptr;
	if (space < ETHER_LNK_HEAD_LEN)
	{
		// won't fail
		head = skb_alloc(sizeof(ether_lnk_t), 1);

		// update the head 
		head->flag |= SK_PK_HEAD;
		head->dgram_len = skb->dgram_len;
		head->dgram_len += ETHER_LNK_HEAD_LEN;
	
		skb->flag &= ~(SK_PK_HEAD | SK_PK_HEAD_DATA);
		skb->flag |= SK_PK_DATA;
		
		list_add_tail(&head->list, &skb->list);		
	}
	else
	{
		skb->flag &= ~(SK_PK_DATA | SK_PK_HEAD);
		skb->flag |= SK_PK_HEAD_DATA;
		skb->dgram_len += ETHER_LNK_HEAD_LEN;
		head = skb;
	}

	head->data_ptr -= ETHER_LNK_HEAD_LEN;
	head->netif = (netif_t *)netif;

	p = sk_get_buff_ptr(head);
	p += head->data_ptr;

	plnk = (ether_lnk_t *)p;

	get_netif_ip_addr(netif0, &my_ip);
	local = ip_eq(ip_addr, &my_ip);
	
	if (!get_ether_mac_addr(netif, &mac))
	{
		skb_free_packet(head);
		return FALSE;
	}
	memcpy(&plnk->mac_src, &mac, MAC_ADDR_LEN);

	// local address, we don't get macaddr by arp.
	if (!local && !get_dest_mac_addr(netif0, gw_ipaddr, &mac))
	{
		skb_free_packet(head);
		return FALSE;
	}
	
	memcpy(&plnk->mac_dst, &mac, MAC_ADDR_LEN);
	plnk->lnk_type = htons(IP_TYPE);

	if (local)
	{
		// put into in queue
		list_add_tail(&netif->in_queue, &head->pkt_list);

		raise_bottom(PCNET32_B);
	}
	else
	{
		list_add_tail(&netif->out_queue, &head->pkt_list);

		/* call the driver */
		(*netif->transmit)(netif);
	}
	
	return TRUE;
}

static void ether_input(netif_t *netif0)
{
	unsigned long flags;
	skbuff_t * skb;
	unsigned char *ptr;
	list_t *node;
	ether_lnk_t *arp;
	int type;
	ether_netif_t *netif = (ether_netif_t *)netif0;
	
	lock(flags);

	while (!list_empty(&netif->in_queue))
	{
		// get one packet
		node = netif->in_queue.next;
		list_del1(node);

		unlock(flags);
		
		skb = list_entry(node, pkt_list, skbuff_t);
		ptr = sk_get_buff_ptr(skb);
		ptr += skb->data_ptr;
		arp = (ether_lnk_t *)ptr;

		type = ntohs(arp->lnk_type);
		switch (type)
		{
			case ARP_TYPE:
				input_arp_packet(skb);
				break;

			case IP_TYPE:
				skb->data_ptr += ETHER_LNK_HEAD_LEN;
				skb->dgram_len -= ETHER_LNK_HEAD_LEN;

				input_ip_packet(skb);
				break;

			case RARP_TYPE:
				skb_free(skb);
				break;

			default:
				skb_free(skb);
				break;
		}
		
		lock(flags);
	}

	unlock(flags);
}

static int ether_ioctl(netif_t *netif0, int request,
				void *data, 
				int openflags)
{
	int error = 0;
	ifreq_t *req = (ifreq_t *)data;
	ether_netif_t *netif = (ether_netif_t *)netif0;
	ether_netif_addr_t *addr = (ether_netif_addr_t *)netif->netif_addr;
	
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
			if (req->ifr_mtu > ETHER_MTU)
			{
				error = -EINVAL;
			}
			else
			{
				netif->netif_mtu = req->ifr_mtu;
			}

			break;
		}
		
		case SIOCGIFHWADDR:
		{
			struct sockaddr *sock;
			sock = &req->ifr_addr;
			sock->sa_family = ARPHRD_ETHER;
			memcpy(&sock->sa_data[0], 
				&addr->ether_addr,
				MAC_ADDR_LEN);

			break;
		}

		case SIOCSIFHWADDR:
		{
			struct sockaddr *sock;
			
			CHECK_PERM();
			sock = &req->ifr_addr;
			
			memcpy(&addr->ether_addr,
				&sock->sa_data[0],
				MAC_ADDR_LEN);
			
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

			if (netif->ioctl(netif, netif->flag) < 0)
			{
				error = -EAGAIN;
			}
						
			break;
		}
	}

	return error;
}

#define IP_ADDR_INVALID(x)	(iptoul(x) == ~0)

static BOOLEAN get_dest_mac_addr(netif_t *netif0,
			const ip_addr_t *dest_ip, 
			mac_addr_t *pmac)
{
	ip_addr_t gw;
	netif_t *netif;

	//printk("%s\n", __FUNCTION__);
	
	netif = get_netif(dest_ip, &gw);

	if (netif != netif0)
	{
		PANIC("BUG: can not happend!\n");
	}
	
	if (IP_ADDR_INVALID(gw))
	{
		memcpy(&gw, dest_ip, IP_ADDR_LEN);
	}

//	print_ip_addr(&gw);
	
	return get_mac_addr_by_arp((ether_netif_t *)netif, &gw, pmac);
}

static netif_t *find_netif_from_alist(mac_addr_t *pmac, list_t *addr_list)
{
	list_t *tmp;
	list_t *pos;
	ether_netif_addr_t *addr;

	foreach (tmp, pos, addr_list)
		addr = list_entry(tmp, list, ether_netif_addr_t);

		if (mac_addr_equal(&addr->ether_addr, pmac))
		{
			return addr->netif;
		}
	endeach(addr_list);
	
	return NULL;
}

ether_netif_t *get_netif_by_mac(mac_addr_t *pmac)
{
	list_t *pos;
	ether_netif_t *netif;
	ether_netif_t *ret;
	netif_addr_t *addr;

	list_foreach_quick(pos, &net_device_list)
	{
		netif = list_entry(pos, list, ether_netif_t);

		if (!sub_str_eq(netif->netif_name, "eth", 3))
		{
			continue;
		}
		
		addr = netif->netif_addr;

		if ((ret = (ether_netif_t *)find_netif_from_alist(pmac, &addr->list)))
		{
			return ret;
		}
	}

	return NULL;
}

BOOLEAN get_ether_boardcast_addr(ether_netif_t *netif, mac_addr_t *pmac)
{
	ether_netif_addr_t *paddr;

	if (!netif || !(paddr = (ether_netif_addr_t *)(netif->netif_addr)))
	{
		return FALSE;
	}
	
	memcpy(pmac, &paddr->ether_bcast_addr, MAC_ADDR_LEN);

	return TRUE;
}

BOOLEAN get_ether_mac_addr(ether_netif_t *netif, mac_addr_t *pmac)
{
	ether_netif_addr_t *paddr;

	if (!netif || !(paddr = (ether_netif_addr_t *)(netif->netif_addr)))
	{
		return FALSE;
	}

	memcpy(pmac, &paddr->ether_addr, MAC_ADDR_LEN);
	return TRUE;
}

ether_netif_t *get_netif_by_irq(int irq)
{
	list_t *pos;
	ether_netif_t *netif;

	list_foreach_quick(pos, &net_device_list)
	{
		netif = list_entry(pos, list, ether_netif_t);

		if (!sub_str_eq(netif->netif_name, "eth", 3))
		{
			continue;
		}

		if (netif->irq_no == irq)
		{
			return netif;
		}
	}

	return NULL;
}

