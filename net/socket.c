#include <cnix/string.h>
#include <cnix/types.h>
#include <cnix/errno.h>
#include <cnix/config.h>
#include <cnix/kernel.h>
#include <cnix/mm.h>
#include <cnix/fs.h>
#include <cnix/net.h>

#define SOCK_socket		1
#define SOCK_bind		2
#define SOCK_connect		3
#define SOCK_listen		4
#define SOCK_accept		5
#define SOCK_getsockname	6
#define SOCK_getpeername	7
#define SOCK_socketpair		8
#define SOCK_send		9
#define SOCK_recv		10
#define SOCK_sendto		11
#define SOCK_recvfrom		12
#define SOCK_shutdown		13
#define SOCK_setsockopt		14
#define SOCK_getsockopt		15
#define SOCK_sendmsg		16
#define SOCK_recvmsg		17

static int accept(int fd, struct sockaddr * addr, int * addr_len)
{
	int ret = 0;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(cklimit(addr) || cklimit(addr_len))
		return -EFAULT;

	if(addr){
		if(!addr_len)
			return -EINVAL;

		if(ckoverflow(addr, *addr_len))
			return -EFAULT;
	}

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	switch(sock->type){
	case SOCK_STREAM:
		if(!sock->tserv || (sock->tserv->state != TCP_LISTEN))
			return -EBADF;

		ret = tcp_accept(sock, (struct sockaddr_in *)addr, addr_len);
		break;
	case SOCK_DGRAM:
		break;
	case SOCK_RAW:
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

static int bind(int fd, const struct sockaddr * addr, int len)
{
	int ret = 0;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;
	struct sockaddr_in * inaddr;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(cklimit(addr))
		return -EFAULT;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;
	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	sock = (socket_t *)inoptr->i_data;
	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	if(sock->state != SOCK_INIT)
		return -EBADF;

	if(len != sizeof(struct sockaddr_in))
		return -EINVAL;

	inaddr = (struct sockaddr_in *)addr;
	if(inaddr->sin_family != AF_INET)
		return -EINVAL;

	if(!inaddr->sin_port)
		return -EINVAL;
	
//	if(inet_port_busy(UDPP, ntohs(inaddr->sin_port)))
//		return -EADDRINUSE;

	if(!ISANY(inaddr->sin_addr)
		&& !match_ifaddr((ip_addr_t *)&inaddr->sin_addr))
			return -EINVAL;

	switch(sock->type){
	case SOCK_STREAM:
		ret = tcp_bind(sock, inaddr);
		break;
	case SOCK_DGRAM:
		ret = udp_bind(sock, inaddr);
		break;
	case SOCK_RAW:
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;
	}

	return ret;
}

static int connect(int fd, const struct sockaddr * addr, int len)
{
	int ret = 0;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;
	struct sockaddr_in * inaddr;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(cklimit(addr))
		return -EFAULT;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;
	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	sock = (socket_t *)inoptr->i_data;
	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	if(sock->state != SOCK_INIT)
		return -EBADF;

	if(len != sizeof(struct sockaddr_in))
		return -EINVAL;

	inaddr = (struct sockaddr_in *)addr;
	if(inaddr->sin_family != AF_INET)
		return -EINVAL;

	if(ISANY(inaddr->sin_addr) || !inaddr->sin_port)
		return -EINVAL;

	switch(sock->type){
	case SOCK_STREAM:
		ret = tcp_connect(sock, inaddr);
		break;
	case SOCK_DGRAM:
		ret = udp_connect(sock, inaddr);
		break;
	case SOCK_RAW:
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;
	}

	return ret;
}

static int getpeername(int fd, struct sockaddr * addr, int * len)
{
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(cklimit(addr) || ckoverflow(addr, *len))
		return -EINVAL;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	if(*len < sizeof(struct sockaddr_in))
		return -EINVAL;

	if(!sock->u.serv)
		return -ENOTCONN;

	switch(sock->type){
	case SOCK_STREAM:
		*(struct sockaddr_in *)addr = sock->tserv->dstaddr;
		break;
	case SOCK_DGRAM:
		*(struct sockaddr_in *)addr = sock->userv->dstaddr;
		break;
	case SOCK_RAW:
		return -EBADF;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return 0;
}

static int getsockname(int fd, struct sockaddr * addr, int * len)
{
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(cklimit(addr) || ckoverflow(addr, *len))
		return -EINVAL;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	if(*len < sizeof(struct sockaddr_in))
		return -EINVAL;

	if(!sock->u.serv)
		return -ENOTCONN;

	switch(sock->type){
	case SOCK_STREAM:
		*(struct sockaddr_in *)addr = sock->tserv->srcaddr;
		break;
	case SOCK_DGRAM:
		*(struct sockaddr_in *)addr = sock->userv->srcaddr;
		break;
	case SOCK_RAW:
		return -EBADF;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return 0;
}

#define MAX_BACKLOG 16

static int listen(int fd, int n)
{
	int ret = 0;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if((n < 0) || (n > MAX_BACKLOG))
		return -EINVAL;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	switch(sock->type){
	case SOCK_STREAM:
		if(!sock->tserv || (sock->tserv->state != TCP_INIT))
			return -EBADF;

		ret = tcp_listen(sock, n);
		break;
	case SOCK_DGRAM:
		return -EBADF;
	case SOCK_RAW:
		return -EBADF;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

static ssize_t recv(int fd, void * buf, size_t n, int flags)
{
	int ret = 0;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if((n < 0) || !buf)
		return -EINVAL;

	if(cklimit(buf) || ckoverflow(buf, n))
		return -EFAULT;

	fp = fget(fd);
	if(!fp)
		return -EBADF;
	
	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	if(!n)
		return 0;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	switch(sock->type){
	case SOCK_STREAM:
		ret = tcp_read(sock, buf, n, flags);
		break;
	case SOCK_DGRAM:
		ret = udp_read(sock, NULL, NULL, buf, n, flags);
		break;
	case SOCK_RAW:
		ret = -EBADF;
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

static ssize_t recvfrom(
	int fd,
	void * buf, size_t n, 
	int flags,
	struct sockaddr * addr, int * addr_len
	)
{
	int ret = 0;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if((n < 0) || !buf)
		return -EINVAL;

	if(cklimit(buf) || ckoverflow(buf, n))
		return -EFAULT;

	if(cklimit(addr) || cklimit(addr_len))
		return -EINVAL;

	if(addr){
		if(!addr_len)
			return -EINVAL;

		if(ckoverflow(addr, *addr_len))
			return -EFAULT;
	}

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	if(!n)
		return 0;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	if(*addr_len < sizeof(struct sockaddr_in))
		return -EINVAL;

	switch(sock->type){
	case SOCK_STREAM:
		ret = tcp_read(sock, buf, n, flags);
		break;
	case SOCK_DGRAM:
		ret = udp_read(
			sock,
			(struct sockaddr_in *)addr, addr_len,
			buf, n,
			flags
			);
		break;
	case SOCK_RAW:
		ret = raw_read(
			sock,
			(struct sockaddr_in *)addr, addr_len,
			buf, n,
			flags
			);
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

static ssize_t recvmsg(int fd, struct msghdr * message, int flags)
{
	int ret = 0;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(cklimit(message) || ckoverflow(message, sizeof(struct msghdr)))
		return -EFAULT;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	switch(sock->type){
	case SOCK_STREAM:
		ret = tcp_recvmsg(sock, message, flags);
		break;
	case SOCK_DGRAM:
		ret = udp_recvmsg(sock, message, flags);
		break;
	case SOCK_RAW:
		ret = raw_recvmsg(sock, message, flags);
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

static ssize_t send(int fd, const void * buf, size_t n, int flags)
{
	int ret = 0;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if((n < 0) || !buf)
		return -EINVAL;

	if(cklimit(buf) || ckoverflow(buf, n))
		return -EFAULT;

	fp = fget(fd);
	if(!fp)
		return -EBADF;
	
	if((fp->f_mode & O_ACCMODE) == O_RDONLY)
		return -EINVAL;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	if(!n)
		return 0;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	switch(sock->type){
	case SOCK_STREAM:
		ret = tcp_write(sock, buf, n, flags);
		break;
	case SOCK_DGRAM:
		ret = udp_write(sock, NULL, buf, n, flags);
		break;
	case SOCK_RAW:
		ret = -EBADF;
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

static ssize_t sendto(
	int fd,
	const void * buf, size_t n,
	int flags,
	const struct sockaddr * addr, int addr_len
	)
{
	int ret = 0;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if((n < 0) || !buf)
		return -EINVAL;

	if(cklimit(buf) || ckoverflow(buf, n))
		return -EFAULT;

	if(cklimit(addr) || ckoverflow(addr, addr_len))
		return -EINVAL;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	if((fp->f_mode & O_ACCMODE) == O_RDONLY)
		return -EINVAL;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	if(!n)
		return 0;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	if(addr_len != sizeof(struct sockaddr_in))
		return -EINVAL;

	switch(sock->type){
	case SOCK_STREAM:
		ret = tcp_write(sock, buf, n, flags);
		break;
	case SOCK_DGRAM:
		ret = udp_write(
			sock, (struct sockaddr_in *)addr, buf, n, flags
			);
		break;
	case SOCK_RAW:
		ret = raw_write(
			sock, (struct sockaddr_in *)addr, buf, n, flags
			);
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

static int sol_setsockopt(
	socket_t * sock, int opt, const void * optval, int optlen
	)
{
	int v = 0, ret = 0;
	struct linger * l = NULL;
	struct timeval * t = NULL;

	if(opt == SO_LINGER){
		if(optlen != sizeof(struct linger))
			return -EINVAL;

		l = (struct linger *)optval;
	}else if((opt == SO_RCVTIMEO) || (opt == SO_SNDTIMEO)){
		if(optlen != sizeof(struct timeval))
			return -EINVAL;	

		t = (struct timeval *)optval;
	}else{
		if(optlen != sizeof(int))
			return -EINVAL;

		v = *(int *)optval;
	}

	switch(opt){
	case SO_BROADCAST: // XXX
		sock->opt.so_broadcast = v;
		break;
	case SO_DEBUG: // XXX
		sock->opt.so_debug = v;
		break;
	case SO_DONTROUTE: // XXX
		sock->opt.so_dontroute = v;
		break;
	case SO_ERROR: // XXX
		ret = -EINVAL;
		break;
	case SO_KEEPALIVE: // XXX
		sock->opt.so_keepalive = v;
		break;
	case SO_LINGER: // XXX
		sock->opt.so_linger = *l;
		break;
	case SO_OOBINLINE:
		sock->opt.so_oobinline = v;
		break;
	case SO_RCVBUF: // XXX
		if(v < (XMT_MSS * 3))
			v = XMT_MSS * 3;

		if(v > DEFAULT_RING_SIZE)
			v = DEFAULT_RING_SIZE;

		sock->opt.so_rcvbuf = v;
		break;
	case SO_SNDBUF: // XXX
		if(v < (XMT_MSS * 3))
			v = XMT_MSS * 3;

		if(v > DEFAULT_RING_SIZE)
			v = DEFAULT_RING_SIZE;

		sock->opt.so_sndbuf = v;
		break;
	case SO_RCVLOWAT: // XXX
		if(v <= 0)
			v = 1;

		if(v > DEFAULT_RING_SIZE)
			v = DEFAULT_RING_SIZE;

		sock->opt.so_rcvlowat = v;
		break;
	case SO_SNDLOWAT: // XXX
		if(v <= 0)
			v = 1;

		if(v > DEFAULT_RING_SIZE)
			v = DEFAULT_RING_SIZE;

		sock->opt.so_sndlowat = v;
		break;
	case SO_RCVTIMEO: // XXX
		sock->opt.so_rcvtimeo = *t;
		break;
	case SO_SNDTIMEO: // XXX
		sock->opt.so_sndtimeo = *t;
		break;
	case SO_REUSEADDR: // XXX
		sock->opt.so_reuseaddr = v;
		break;
	case SO_REUSEPORT: // XXX
		sock->opt.so_reuseport = v;
		break;
	case SO_TYPE:
		ret = -EINVAL;
		break;
	case SO_USELOOPBACK:
		ret = -EINVAL;
		break;
	case SO_ACCEPTCONN:
	case SO_TIMESTAMP:
	case SO_ACCEPTFILTER:
	case SO_PRIVSTATE:
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

static int setsockopt(
	int fd,
	int level,
	int opt, const void * optval, int optlen
	)
{
	int ret = -ENOTSUP;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if((optlen < 0) || !optval)
		return -EINVAL;

	if(cklimit(optval) || ckoverflow(optval, optlen))
		return -EFAULT;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	switch(level){
	case SOL_SOCKET:
		ret = sol_setsockopt(sock, opt, optval, optlen);
		break;
	case IPPROTO_IP:
		break;
	case IPPROTO_TCP:
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

static int sol_getsockopt(
	socket_t * sock, int opt, void * optval, int * optlen
	)
{
	int * v = NULL, ret = 0;
	struct linger * l = NULL;
	struct timeval * t = NULL;

	if(opt == SO_LINGER){
		l = (struct linger *)optval;
		*optlen = sizeof(struct linger);
	}else if((opt == SO_RCVTIMEO) || (opt == SO_SNDTIMEO)){
		t = (struct timeval *)optval;
		*optlen = sizeof(struct timeval);
	}else{
		v = (int *)optval;
		*optlen = sizeof(int);
	}

	if(ckoverflow(optval, *optlen))
		return -EINVAL;

	switch(opt){
	case SO_BROADCAST:
		*v = sock->opt.so_broadcast;
		break;
	case SO_DEBUG:
		*v = sock->opt.so_debug;
		break;
	case SO_DONTROUTE:
		*v = sock->opt.so_dontroute;
		break;
	case SO_ERROR:
		*v = sock->opt.so_error;
		break;
	case SO_KEEPALIVE:
		*v = sock->opt.so_keepalive;
		break;
	case SO_LINGER:
		*l= sock->opt.so_linger;
		break;
	case SO_OOBINLINE:
		*v = sock->opt.so_oobinline;
		break;
	case SO_RCVBUF:
		*v = sock->opt.so_rcvbuf;
		break;
	case SO_SNDBUF:
		*v = sock->opt.so_sndbuf;
		break;
	case SO_RCVLOWAT:
		*v = sock->opt.so_rcvlowat;
		break;
	case SO_SNDLOWAT:
		*v = sock->opt.so_sndlowat;
		break;
	case SO_RCVTIMEO:
		*t = sock->opt.so_rcvtimeo;
		break;
	case SO_SNDTIMEO:
		*t = sock->opt.so_sndtimeo;
		break;
	case SO_REUSEADDR:
		*v = sock->opt.so_reuseaddr;
		break;
	case SO_REUSEPORT:
		*v = sock->opt.so_reuseport;
		break;
	case SO_TYPE:
		*v = sock->type;
		break;
	case SO_USELOOPBACK:
		ret = -EINVAL;
		break;
	case SO_ACCEPTCONN:
	case SO_TIMESTAMP:
	case SO_ACCEPTFILTER:
	case SO_PRIVSTATE:
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

static int getsockopt(int fd, int level, int opt, void * optval, int * optlen)
{
	int ret = -ENOTSUP;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if((optlen < 0) || !optval)
		return -EINVAL;

	if(!optlen)
		return -EINVAL;

	if(cklimit(optval))
		return -EFAULT;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	switch(level){
	case SOL_SOCKET:
		ret = sol_getsockopt(sock, opt, optval, optlen);
		break;
	case IPPROTO_IP:
		break;
	case IPPROTO_TCP:
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

static int shutdown(int fd, int how)
{
	int ret = 0;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if((how != SHUT_RD) && (how != SHUT_WR) && (how != SHUT_RDWR))
		return -EINVAL;

	if(ckfdlimit(fd))
		return -EINVAL;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	switch(sock->type){
	case SOCK_STREAM:
		ret = tcp_shutdown(sock, how);
		break;
	case SOCK_DGRAM:
		ret = -ENOTCONN;
		break;
	case SOCK_RAW:
		ret = -ENOTCONN;
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

ssize_t socket_read(
	struct inode * inoptr,
	char * buffer,
	size_t count,
	int * error,
	int flags
	)
{
	int ret = 0, flags1;
	socket_t * sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	flags1 = 0;
	if(flags1 & O_NDELAY)
		flags1 = MSG_DONTWAIT;

	switch(sock->type){
	case SOCK_STREAM:
		ret = tcp_read(sock, buffer, count, flags1);
		break;
	case SOCK_DGRAM:
		ret = udp_read(sock, NULL, NULL, buffer, count, flags1);
		break;
	case SOCK_RAW:
		ret = -EBADF;
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

ssize_t socket_write(
	struct inode * inoptr,
	const char * buffer,
	size_t count,
	int * error,
	int flags
	)
{
	int ret = 0, flags1;
	socket_t * sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	flags1 = 0;
	if(flags1 & O_NDELAY)
		flags1 = MSG_DONTWAIT;

	switch(sock->type){
	case SOCK_STREAM:
		ret = tcp_write(sock, buffer, count, flags1);
		break;
	case SOCK_DGRAM:
		ret = udp_write(sock, NULL, buffer, count, flags1);
		break;
	case SOCK_RAW:
		ret = -EBADF;
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	if(ret < 0){
		*error = ret;
		return 0;
	}

	return ret;
}

void socket_close(struct inode * inoptr)
{
	socket_t * sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happne\n");

	switch(sock->type){
	case SOCK_DGRAM:
		udp_close(sock);
		break;
	case SOCK_STREAM:
		tcp_close(sock);
		break;
	case SOCK_RAW:
		raw_close(sock);
		break;
	default:
		break;
	}

	sock_free(sock);

	inoptr->i_data = NULL;
}

static int socket(int domain, int type, int protocol)
{
	int error, ret = 0;
	struct inode * inoptr;
	socket_t * sock;

	switch(domain){
	case AF_INET:
		switch(protocol){
		case IPPROTO_UDP:
			if(type != SOCK_DGRAM)
				return -EINVAL;
			break;
		case IPPROTO_TCP:
			if(type != SOCK_STREAM)
				return -EINVAL;
			break;
		case IPPROTO_ICMP:
			if(type != SOCK_RAW)
				return -EINVAL;
			break;
		case IPPROTO_IP:
			break;
		default:
			return -ENOTSUP;
		}

		if(type == SOCK_RAW){
			if(current->euid != SU_UID){
				ret = -EPERM;
				break;
			}

			if(protocol != IPPROTO_ICMP)
				return -ENOTSUP;
		}

		inoptr = iget(NODEV, 0);

		sock = sock_alloc(AF_INET, type, protocol);
		sock->inode = inoptr;

		inoptr->i_mode |= I_SOCK;
		inoptr->i_data = sock;

		ret = fdget(0, O_RDWR, inoptr, &error);
		if(ret < 0){
			iput(inoptr); // iput will call socket_close
			//sock_free(sock);
			return error;
		}

		switch(type){
		case SOCK_STREAM:
			break;
		case SOCK_DGRAM:
			break;
		case SOCK_RAW:
			error = raw_open(sock);
			if(error < 0){
				iput(inoptr);
				return error;
			}
			break;
		default:
			ret = -ENOTSUP;
			break;
 		}

		inoptr->i_count++;
		iput(inoptr);

		break;
	case AF_UNIX:
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

static int socketpair(int domain, int type, int protocol, int fds[2])
{
	return -EAFNOSUPPORT;
}

static ssize_t sendmsg(int fd, const struct msghdr * message, int flags)
{
	int ret = 0;
	struct filp * fp;
	struct inode * inoptr;
	socket_t * sock;

	if(ckfdlimit(fd))
		return -EINVAL;

	if(cklimit(message) || ckoverflow(message, sizeof(struct msghdr)))
		return -EFAULT;

	fp = fget(fd);
	if(!fp)
		return -EBADF;

	if((fp->f_mode & O_ACCMODE) == O_RDONLY)
		return -EINVAL;

	inoptr = fp->f_ino;

	if(!S_ISSOCK(inoptr->i_mode))
		return -ENOTSOCK;

	sock = (socket_t *)inoptr->i_data;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happen\n");

	switch(sock->type){
	case SOCK_STREAM:
		ret = tcp_sendmsg(sock, message, flags);
		break;
	case SOCK_DGRAM:
		ret = udp_sendmsg(sock, message, flags);
		break;
	case SOCK_RAW:
		ret = raw_sendmsg(sock, message, flags);
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;	
	}

	return ret;
}

int sys_socketcall(int call, unsigned long * args)
{
	int ret;

	if(cklimit(args))
		return -EFAULT;

	switch(call){
	case SOCK_socket:
		ret = socket((int)args[0], (int)args[1], (int)args[2]);
		break;
	case SOCK_bind:
		ret = bind(
			(int)args[0],
			(const struct sockaddr *)args[1],
			(int)args[2]
			);
		break;
	case SOCK_connect:
		ret = connect(
			(int)args[0],
			(const struct sockaddr *)args[1],
			(int)args[2]
			);
		break;
	case SOCK_listen:
		ret = listen((int)args[0], (int)args[1]);
		break;
	case SOCK_accept:
		ret = accept(
			(int)args[0],
			(struct sockaddr *)args[1],
			(int *)args[2]
			);
		break;
	case SOCK_getsockname:
		ret = getsockname(
			(int)args[0],
			(struct sockaddr *)args[1],
			(int *)args[2]
			);
		break;
	case SOCK_getpeername:
		ret = getpeername(
			(int)args[0],
			(struct sockaddr *)args[1],
			(int *)args[2]
			);
		break;
	case SOCK_socketpair:
		ret = socketpair(
			(int)args[0],
			(int)args[1],
			(int)args[2],
			(int *)args[3]
			);
		break;
	case SOCK_send:
		ret = send(
			(int)args[0],
			(const void *)args[1],
			(size_t)args[2],
			(int)args[3]
			);
		break;
	case SOCK_recv:
		ret = recv(
			(int)args[0],
			(void *)args[1],
			(size_t)args[2],
			(int)args[3]
			);
		break;
	case SOCK_sendto:
		ret = sendto(
			(int)args[0],
			(const void *)args[1], (size_t)args[2], 
			(int)args[3],
			(const struct sockaddr *)args[4], (int)args[5]
			);
		break;
	case SOCK_recvfrom:
		ret = recvfrom(
			(int)args[0],
			(void *)args[1], (size_t)args[2], 
			(int)args[3],
			(struct sockaddr *)args[4], (int *)args[5]
			);
		break;
	case SOCK_shutdown:
		ret = shutdown((int)args[0], (int)args[1]);
		break;
	case SOCK_setsockopt:
		ret = setsockopt(
			(int)args[0],
			(int)args[1],
			(int)args[2], (const void *)args[3], (int)args[4]
			);
		break;
	case SOCK_getsockopt:
		ret = getsockopt(
			(int)args[0],
			(int)args[1],
			(int)args[2], (void *)args[3], (int *)args[4]
			);
		break;
	case SOCK_sendmsg:
		ret = sendmsg(
			(int)args[0],
			(const struct msghdr *)args[1], 
			(int)args[2]
			);
		break;
	case SOCK_recvmsg:
		ret = recvmsg(
			(int)args[0],
			(struct msghdr *)args[1], 
			(int)args[2]
			);
		break;
	default:
		return -ENOSYS;
	}

	return ret;
}

BOOLEAN sock_can_read(socket_t * sock)
{
	BOOLEAN ret = FALSE;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happne\n");

	switch(sock->type){
	case SOCK_DGRAM:
		ret = udp_can_read(sock);
		break;
	case SOCK_STREAM:
		ret = tcp_can_read(sock);
		break;
	case SOCK_RAW:
		ret = raw_can_read(sock);
		break;
	default:
		break;
	}

	return ret;
}

BOOLEAN sock_can_write(socket_t * sock)
{
	BOOLEAN ret = FALSE;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happne\n");

	switch(sock->type){
	case SOCK_DGRAM:
		ret = udp_can_write(sock);
		break;
	case SOCK_STREAM:
		ret = tcp_can_write(sock);
		break;
	case SOCK_RAW:
		ret = raw_can_write(sock);
		break;
	default:
		break;
	}

	return ret;
}

BOOLEAN sock_can_except(socket_t * sock)
{
	BOOLEAN ret = FALSE;

	if(sock->family != AF_INET)
		DIE("BUG: cannot happne\n");

	switch(sock->type){
	case SOCK_DGRAM:
		break;
	case SOCK_STREAM:
		ret = tcp_can_except(sock);
		break;
	case SOCK_RAW:
		break;
	default:
		break;
	}

	return ret;
}
