#include <asm/io.h>
#include <asm/system.h>
#include <cnix/types.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/errno.h>
#include <cnix/mm.h>
#include <cnix/netif.h>
#include <cnix/net.h>
#include <cnix/route.h>
#include <cnix/sockios.h>

#define str_eq(x, y)	(strcmp(x, y) == 0)

// the list head of network devices
list_t net_device_list = { NULL, NULL };

// compare the network address.
static netif_t *get_netif_from_alist(ip_addr_t * ip, list_t *addr_list)
{
	list_t *tmp;
	list_t *pos;
	netif_addr_t *addr;
	ip_addr_t net1;
	ip_addr_t net2;

	foreach (tmp, pos, addr_list)
	{
		addr = list_entry(tmp, list, netif_addr_t);

		netmask_ip(ip, &addr->netmask, &net1);
		netmask_ip(&addr->ip_addr, &addr->netmask, &net2);
		if (ip_eq(&net1, &net2))
		{
			return addr->netif;
		}
	}
	endeach(addr_list);

	return NULL;
}

// no route
netif_t * get_outif(ip_addr_t * ip)
{
	list_t *pos;
	netif_t *netif;
	netif_t *ret;
	netif_addr_t *addr;

	list_foreach_quick(pos, &net_device_list)
	{
		netif = list_entry(pos, list, netif_t);
		addr = netif->netif_addr;

		if ((ret = get_netif_from_alist(ip, &addr->list)))
		{
			// ASSERT(netif == ret)
			return ret;
		}
	}

	return NULL;
}

netif_t *find_netif_by_name(const char *name)
{
	list_t *pos;
	netif_t *netif;

	list_foreach_quick(pos, &net_device_list)
	{
		netif = list_entry(pos, list, netif_t);

		if (str_eq(netif->netif_name, name))
		{
			return netif;
		}
	}

	return NULL;
}

// TRUE if the net interface dosn't exist
// FALSE if the net interface exists.
BOOLEAN add_netif(netif_t *net)
{
	list_t *pos;
	netif_t *netif;
	BOOLEAN ok = FALSE;
	
	list_foreach_quick(pos, &net_device_list)
	{
		netif = list_entry(pos, list, netif_t);

		if (str_eq(netif->netif_name, net->netif_name))
		{
			ok = TRUE;
			break;
		}
	}

	if (ok)
	{
		list_del1(pos);	// delete the old netif
	}
	
	list_add_tail(&net_device_list, &net->list);

	return ok;
}

BOOLEAN del_netif(netif_t *net)
{
	list_t *pos;
	netif_t *netif;
	BOOLEAN ok = FALSE;
	
	list_foreach_quick(pos, &net_device_list)
	{
		netif = list_entry(pos, list, netif_t);

		if (str_eq(netif->netif_name, net->netif_name))
		{
			ok = TRUE;
			break;
		}
	}

	if (ok)
	{
		list_del1(&netif->list);	// delete the old netif
	}

	return ok;
}

BOOLEAN get_netif_ip_addr(netif_t *netif, ip_addr_t *pip)
{
	netif_addr_t *paddr;

	if (!netif)
	{
		return FALSE;
	}

	paddr = netif->netif_addr;
	memcpy(pip, &paddr->ip_addr, IP_ADDR_LEN);

	return TRUE;
}

BOOLEAN get_netif_netmask(netif_t *netif, ip_addr_t *mask)
{
	netif_addr_t *paddr;

	if (!netif)
	{
		return FALSE;
	}

	paddr = netif->netif_addr;
	memcpy(mask, &paddr->netmask, IP_ADDR_LEN);

	return TRUE;
}

//
void netif_initialize(netif_t *netif)
{
	if (!netif)
	{
		return;
	}

	if (!net_device_list.next)
	{
		list_init(&net_device_list);
	}

	list_init(&netif->list);
	netif->rx_byte_count = 0;
	netif->rx_frame_count = 0;
	netif->rx_error_count = 0;
	netif->tx_byte_count = 0;
	netif->tx_frame_count = 0;
	netif->tx_byte_count = 0;

	list_init(&netif->in_queue);
	list_init(&netif->out_queue);
}

void process_all_input(void)
{
	list_t *pos;
	netif_t *netif;

	list_foreach_quick(pos, &net_device_list)
	{
		netif = list_entry(pos, list, netif_t);

		(*netif->netif_input)(netif);
	}
}

// no route
BOOLEAN match_ifaddr(ip_addr_t * ip)
{
	return get_outif(ip) != NULL;
}

static int get_ifconf(ifreq_t array[], int n)
{
	list_t *pos;
	netif_t *netif;
	ifreq_t *preq;
	netif_addr_t *addr;
	int i;

	i = 0;
	preq = &array[0];
	//memset(preq, 0, n * sizeof(ifreq_t));
	
	list_foreach_quick (pos, &net_device_list)
	{
		netif = list_entry(pos, list, netif_t);
		addr = netif->netif_addr;
		
		strcpy(preq->ifr_name, netif->netif_name);
		preq->ifr_addr.sa_family = AF_INET;
		memcpy(&preq->ifr_addr.sa_data[0],
			&addr->ip_addr,
			IP_ADDR_LEN);

		preq++;
		i++;
		
		if (i >= n)
		{
			break;
		}
	}

	return i;
}

static int do_with_netifaddr(ifreq_t *req, int op)
{
	list_t *pos;
	netif_t *netif;
	netif_addr_t *addr;
	struct sockaddr  *sock =  &req->ifr_addr;
	struct sockaddr_in *sock_in;
	int error = 0;

	sock_in = (struct sockaddr_in *)sock;
	
	list_foreach_quick (pos, &net_device_list)
	{
		netif = list_entry(pos, list, netif_t);
		addr = netif->netif_addr;
		
		if (str_eq(netif->netif_name, req->ifr_name))
		{
			switch (op)
			{
				case SIOCGIFADDR:
					memcpy(&sock_in->sin_addr,
						&addr->ip_addr,
						IP_ADDR_LEN);
					sock_in->sin_family = AF_INET;
					break;
					
				case SIOCSIFADDR:
					CHECK_PERM();
					memcpy(&addr->ip_addr,
						&sock_in->sin_addr,
						IP_ADDR_LEN);
					break;

				case SIOCGIFNETMASK:
					memcpy(&sock_in->sin_addr,
						&addr->netmask,
						IP_ADDR_LEN);
					sock_in->sin_family = AF_INET;
					break;
					
				case SIOCSIFNETMASK:
					CHECK_PERM();
					memcpy(&addr->netmask,
						&sock_in->sin_addr,
						IP_ADDR_LEN);
					break;

				case SIOCGIFBRDADDR:
					memcpy(&sock_in->sin_addr,
						&addr->bcast_addr,
						IP_ADDR_LEN);
					sock_in->sin_family = AF_INET;
					break;

				case SIOCSIFBRDADDR:
					CHECK_PERM();
					memcpy(&addr->bcast_addr,
						&sock_in->sin_addr,
						IP_ADDR_LEN);
					break;

				default:
					error = netif->netif_ioctl(netif,
							op,
							req,
							0);
			}

			return error;
		}
	}

	return -EADDRNOTAVAIL;
}

static int do_with_common(ifreq_t *req, int op)
{
	list_t *pos;
	netif_t *netif;
	int error = 0;

	list_foreach_quick (pos, &net_device_list)
	{
		netif = list_entry(pos, list, netif_t);

		if (str_eq(netif->netif_name, req->ifr_name))
		{
			error = netif->netif_ioctl(netif, op, req, 0);

			return error;
		}
	}

	return -EADDRNOTAVAIL;
}


int do_with_netif_ioctl(int request, void *data)
{
	int error = 0;
	
	switch (request)
	{
		case SIOCADDRT:
		case SIOCDELRT:
		{
			struct rtentry *entry = (struct rtentry *)data;

			CHECK_PERM();			
			error = do_with_route_entry(entry, request);

			return error;
		}
			
		case SIOCGIFCONF:
		{
			// get the netif name
			struct ifconf *ifc;
			int n;

			ifc = (struct ifconf *)data;
#if 0			
			if (!verify_area(ifc->ifc_buf, ifc->ifc_len, M_RW))
			{
				return -EFAULT;
			}
#endif
			n = get_ifconf(ifc->ifc_req, 
					ifc->ifc_len / sizeof(ifconf_t));
	
			ifc->ifc_len = n * sizeof(ifreq_t);
		
			return n;
		}

		case SIOCGIFMTU:
		case SIOCSIFMTU:
		case SIOCGIFFLAGS:
		case SIOCSIFFLAGS:
		{
			ifreq_t *req = (ifreq_t *)data;
			
			return do_with_common(req, request);
		}
		
		case SIOCGIFADDR:
		case SIOCSIFADDR:
		case SIOCGIFBRDADDR:
		case SIOCSIFBRDADDR:
		case SIOCGIFNETMASK:
		case SIOCSIFNETMASK:
		case SIOCGIFHWADDR:
		case SIOCSIFHWADDR:
		case SIOCSIFHWBROADCAST:
		{
			ifreq_t *req = (ifreq_t *)data;
			
			error = do_with_netifaddr(req, request);
			return error;
		}

		default:
			error = -ENOSYS;
			break;
	}

	return error;
}

