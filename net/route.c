#include <cnix/types.h>
#include <cnix/kernel.h>
#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/errno.h>
#include <cnix/driver.h>
#include <cnix/mm.h>
#include <cnix/net.h>
#include <cnix/route.h>
#include <cnix/sockios.h>
#include <cnix/syslog.h>

#define DEFAULT_ROUTE	0
#define HOST_ROUTE	(~0)

#define IS_DEFAULT_ROUTE(addr)	(iptoul(addr) == DEFAULT_ROUTE)
#define IS_NO_ROUTE(addr)	(iptoul(addr) == HOST_ROUTE)


#define R_UP		RTF_UP		// route is up
#define R_HOST		RTF_HOST	// route is a host
#define R_GATEWAY	RTF_GATEWAY	// route is a gateway.

static list_t route_list = { NULL, NULL };

//static route_t route_local;
//static route_t route_remote;

// route is initialzed after netif.
void init_route(void)
{
	// configure by route add & route del
#if 0
	route_t *plocal;
	route_t *premote;
	ip_addr_t dst = {{192, 168, 23, 0}};
	ip_addr_t mask = {{255, 255, 255, 0}};
	ip_addr_t gw = {{192, 168, 23, 1}};
#endif

	list_init(&route_list);

#if 0

	plocal = (route_t *)kmalloc(sizeof(route_t), 0); //&route_local;
	premote = (route_t *)kmalloc(sizeof(route_t), 0); //&route_remote;

	list_init(&plocal->list);
	
	memcpy(&plocal->dest, &dst, IP_ADDR_LEN);
	memcpy(&plocal->netmask, &mask, IP_ADDR_LEN);
	// invalid the gateway address.
	mask.ip[IP_ADDR_LEN - 1] = 255;
	memcpy(&plocal->gateway, &mask, IP_ADDR_LEN);

	plocal->flag = R_UP;
	plocal->netif = get_outif(&dst);

	add_route(plocal);

	list_init(&premote->list);
	memset(&premote->dest, 0, IP_ADDR_LEN);
	memset(&premote->netmask, 0, IP_ADDR_LEN);
	memcpy(&premote->gateway, &gw, IP_ADDR_LEN);
	premote->flag = R_UP | R_GATEWAY;
	premote->netif = get_outif(&gw);

	add_route(premote);	
#endif	
}

static list_t *get_route_entry(route_t *r)
{
	list_t *pos;
	route_t *t;
	
	list_foreach_quick(pos, &route_list)
	{
		t = list_entry(pos, list, route_t);

		if (ip_eq(&t->dest, &r->dest))
		{
			return pos;
		}
	}

	return NULL;
}

// add a route entry
BOOLEAN add_route(route_t *r)
{
	list_t *tmp;
	BOOLEAN ok = TRUE;
	char buffer[32];

	if ((tmp = get_route_entry(r)))
	{
		do_syslog(LOG_KERN, "warning: dest host: ");
		print_buff_ip_addr(buffer, &r->dest);
		do_syslog(LOG_KERN, "%s", buffer);
		do_syslog(LOG_KERN, " exist!\n");

		ok = FALSE;
		
		return ok;
	}

	list_add_tail(&route_list, &r->list);

	return ok;
}

BOOLEAN del_route(route_t *r)
{
	list_t *tmp;
	route_t *old;
	BOOLEAN ok = FALSE;
	
	if ((tmp = get_route_entry(r)))
	{
		list_del1(tmp);

		// free the memory.
		old = list_entry(tmp, list, route_t);
		kfree(old);
		
		ok = TRUE;
	}

	return ok;
}

// from route
netif_t *get_netif(const ip_addr_t *dest, ip_addr_t *gw)
{
	list_t *pos;
	route_t *t;
	ip_addr_t net;

	list_foreach_quick(pos, &route_list)
	{
		t = list_entry(pos, list, route_t);

		if (t->flag & R_HOST)
		{
			if (ip_eq(&t->dest, dest))
			{
				memcpy(gw, &t->gateway, IP_ADDR_LEN);
				return t->netif;
			}

			continue;
		}
		
		netmask_ip(dest, &t->netmask, &net);
		if (ip_eq(&net, &t->dest))
		{
			memcpy(gw, dest, IP_ADDR_LEN);
			return t->netif;
		}

		if (IS_DEFAULT_ROUTE(t->dest))
		{
			memcpy(gw, &t->gateway, IP_ADDR_LEN);
			return t->netif;
		}
	}

	return NULL;
}

// from route
netif_t *find_outif(ip_addr_t *dest, ip_addr_t *gw_ipaddr)
{
	ip_addr_t gw;

	if(gw_ipaddr)
		return get_netif(dest, gw_ipaddr);

	return get_netif(dest, &gw);
}

static __inline__ BOOLEAN check_af_inet(struct sockaddr *x)
{
#if 0
	int i;
	
	printk("sa_family = %d\n", x->sa_family);
	for (i = 0; i < 8; i++)
		printk("%02d ", x->sa_data[i] & 0xFF);
	printk("\n");
#endif	
	
	return x->sa_family == AF_INET;
}

static BOOLEAN check_local_network(ip_addr_t *dst, netif_addr_t *addr)
{
	ip_addr_t net1;
	ip_addr_t net2;

	netmask_ip(dst, &addr->netmask, &net1);
	netmask_ip(&addr->ip_addr, &addr->netmask, &net2);

	return ip_eq(&net1, &net2);
}

int do_with_route_entry(struct rtentry *ent, int op)
{
	route_t *r;
	int error = 0;
	struct sockaddr_in *sock;

	r = (route_t *)kmalloc(sizeof(route_t), PageWait);
	if (!r)
	{
		return -ENOMEM;
	}

	memset(r, 0, sizeof(route_t));
	list_init(&r->list);

	if (!check_af_inet(&ent->rt_dst))
	{
		error = -EINVAL;
		goto do_error;
	}

	sock = (struct sockaddr_in *)&ent->rt_dst;
	memcpy(&r->dest, &sock->sin_addr.s_addr,
		IP_ADDR_LEN);

	// destination is host or default route has no netmask
	if (!(ent->rt_flags & R_HOST) && !IS_DEFAULT_ROUTE(&r->dest))
	{
		// destination is net
		if (!check_af_inet(&ent->rt_genmask))
		{
			error = -EINVAL;
			goto do_error;
		}
		sock = (struct sockaddr_in *)&ent->rt_genmask;
		memcpy(&r->netmask, &sock->sin_addr.s_addr,
			IP_ADDR_LEN);

	}
	else
	{
		memset(&r->netmask, 0xFF, IP_ADDR_LEN);
	}
	
	r->flag = ent->rt_flags;
	if (ent->rt_dev)
	{
		r->netif = find_netif_by_name(ent->rt_dev);
	}
	else
	{
		if (IS_DEFAULT_ROUTE(&r->dest))
		{
			// Check the gateway
			if (!check_af_inet(&ent->rt_gateway))
			{
				error = -EINVAL;
				goto do_error;
			}
			
			sock = (struct sockaddr_in *)&ent->rt_gateway;
			memcpy(&r->gateway, &sock->sin_addr.s_addr,
				IP_ADDR_LEN);
			r->netif = get_outif(&r->gateway);
		}
		else
		{
			r->netif = get_outif(&r->dest);
		}
	}

	if (!r->netif)
	{
		error = -EINVAL;
		goto do_error;
	}

	if (!check_local_network(&r->dest, r->netif->netif_addr))
	{
		if (!check_af_inet(&ent->rt_gateway))
		{
			error = -EINVAL;
			goto do_error;
		}
		
		sock = (struct sockaddr_in *)&ent->rt_gateway;
		memcpy(&r->gateway, &sock->sin_addr.s_addr,
			IP_ADDR_LEN);
	}
	else
	{
		memset(&r->gateway, 0xFF, IP_ADDR_LEN);
	}
	
	switch (op)
	{
		case SIOCADDRT:
		{
			if (add_route(r))
			{
				return 0;
			}
			
			error = -EEXIST;			
			break;
		}

		case SIOCDELRT:
		{
			if (!del_route(r))
			{
				error = -ENOENT;
			}
			
			break;
		}

		default:
			error = -ENOTSUP;
			break;

	}

do_error:
	kfree(r);
	return error;
}


static void print_flag(int flag)
{
	char str[32] = { 0 };
	int i;

	i = 0;
	if (flag & R_UP)
	{
		str[i++] = 'U';
	}

	if (flag & R_GATEWAY)
	{
		str[i++] = 'G';
	}

	if (flag & R_HOST)
	{
		str[i++] = 'H';
	}

	printk("%s", str);
}


void print_route_table(void)
{
	list_t *pos;
	route_t *t;

	printk("\n\nKernel Ip Routing Table:\n");
	printk("Destination\tGateway\t\tGenmask\t\tFlags\tIface\n");
	
	list_foreach_quick(pos, &route_list)
	{
		t = list_entry(pos, list, route_t);

		if (IS_DEFAULT_ROUTE(t->dest))
		{
			printk("default");
			printk("\t\t");
		}
		else
		{
			print_ip_addr(&t->dest);
			printk("\t");
		}
		
		if (IS_NO_ROUTE(t->gateway))
		{
			printk("*");
			printk("\t\t");
		}
		else
		{
			print_ip_addr(&t->gateway);
			printk("\t");
		}

		if (IS_NO_ROUTE(t->netmask))
		{
			printk("*\t\t");
		}
		else
		{
			print_ip_addr(&t->netmask);
			printk("\t");
		}
		
		
		print_flag(t->flag);
		printk("\t");
		
		printk("%s\n", t->netif->netif_name);		
	}

}

