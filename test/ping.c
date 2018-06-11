/*
 * ping source code distribute by cpu || digger.
 * for unix family only. compil and link success in sco unix.
 * i think linux no problem too. u can try it.
 * before read this code, you shoud know about the principle of
 * tcp/ip, especially icmp protocol, u also should also know some
 * about BSD socket API, and unix system signal programming.
 *
 * cc -o ping ping.c -lsocket, then u will get executable file,
 * but must act as root when cc it, and then set euid attribute
 * for this ping, then u can execute it as common user.
 * because only root can have authority to creat raw socket.
 *
 * i love socket, if so do u,
 * call me, cpu == digger
 */

#include <stdio.h>   
#include <errno.h>   
#include <sys/time.h>   
#include <sys/signal.h>   
#include <sys/param.h>   
#include <sys/socket.h>   
#include <netinet/in.h>   
#include <netinet/ip.h>   
#include <netinet/ip_icmp.h>   
#include <ctype.h>   
#include <netdb.h>   
#include <stdlib.h>
#include <string.h>
#include <signal.h>

# define ICMP_ECHO 8 /* icmp echo requir */
# define ICMP_ECHOREPLY 0 /* icmp echo reply */
# define ICMP_HEADSIZE 8 /* icmp packet header size */
# define IP_HEADSIZE 20 /* ip packet header size */

typedef struct tagIpHead /* icmp packet header */
{
	u_char ip_verlen; /* ip version and ip header lenth*/
	u_char ip_tos; /* ip type of service */
	u_short ip_len; /* ip packet lenghth */
	u_short ip_id; /* ip packet identification */
	u_short ip_fragoff; /* ip packet fragment and offset */
	u_char ip_ttl; /* ip packet time to live */
	u_char ip_proto; /* ip packet protocol type */
	u_short ip_chksum; /* ip packet header checksum */
	u_long ip_src_addr; /* ip source ip adress */
	u_long ip_dst_addr; /* ip destination ip adress */
} IPHEAD;

typedef struct tagIcmpHead /* icmp header */
{
	u_char type; /* icmp service type */
	/* 8 echo require, 0 echo reply */
	u_char code; /* icmp header code */
	u_short chksum; /* icmp header chksum */
	u_short id; /* icmp packet identification */
	u_short seq; /* icmp packet sequent */
	u_char data[1]; /* icmp data, use as pointer */
} ICMPHEAD;

u_short ChkSum( u_short * pIcmpData, int iDataLen )
/* for check sum of icmp header */
{
	long iSum;

	iSum = 0;

	while ( iDataLen > 1 ) { /* xor the next unsigned int data */
		iSum += *pIcmpData++;
		if(iSum & 0x80000000)
			iSum = (iSum & 0xffff) + (iSum >> 16);
		iDataLen -= 2;
	}

	if ( iDataLen == 1 ) { /* the rest odd byte */
		iSum += *(u_char *)pIcmpData;
	}

	while(iSum >> 16)
		iSum = (iSum & 0xffff) + (iSum >> 16);

	return(~iSum);
} 

long time_now() /* return time passed by */
		/* since 1970.1.1 00:00:00, */
		/* in 1/1000000 second */
{
	struct timeval now;
	long lPassed;

	gettimeofday(&now, 0);
	lPassed = now.tv_sec * 1000000 + now.tv_usec;
	/* now.tv_sec in second */
	/* now.tv_usec in 1/1000000 second */
	return lPassed;
}

char* host; /* destination host */
char* prog; /* program name */
extern errno; /* system global parameter */
long lSendTime; /* each time when send, change it */
u_short seq; /* the icmp packet seqence */
int iTimeOut; /* time out parameter */
int sock, sent, recvd, max, min, total;
/* sent : icmp packet already sent */
/* recvd: proper icmp packet received */
/* max, min: max min round trip time */
/* total: total round trip time */
/* store to calculate average */
u_long lHostIp; /* host ip adress */
struct sockaddr_in it; /* destination host information */

void ping(int);
void pstat(int);

int main(int argc, char** argv)
{
	struct hostent* h;
	char buf[200];
	char dst_host[32];
	int i, namelen;
	IPHEAD* pIpHead;
	ICMPHEAD* pIcmpHead;
	
	if (argc < 2) { /* ping the destination host */
			/* every timeout second */
			/* default timeout is 1 second */
	
		printf("usage: %s [-timeout] host|IPn", argv[0]);
		exit(0);
	}

	prog = argv[0];
	host = argc == 2 ? argv[1] : argv[2];
	iTimeOut = argc == 2 ? 1 : atoi(argv[1]);
	
	/* creat the raw socket for icmp */
	
	if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		perror("socket");
		exit(2);
	}
	
	/* set destination host information */
	
	bzero(&it, sizeof(it));
	it.sin_family = AF_INET;
	
	/* check host format */
	
	if ( ( lHostIp = inet_addr(host) ) != INADDR_NONE ) {
		/* is available ip adress */
		it.sin_addr.s_addr = lHostIp;
		strcpy( dst_host, host );
	}
#if 0
	else if ( h = gethostbyname(host) ) {
		/* is available host name */
		/* from hosts file of local host */
		/* or from DNS */ 
		bcopy(h->h_addr, &it.sin_addr, h->h_length);
		sprintf( dst_host, "%s (%s)", host,
		inet_ntoa(it.sin_addr) ); 
	}
#endif
	else {
		/* bad ip adress or host name */
		/* exit */
		fprintf( stderr, "bad IP or hostn" );
		exit(3);
	}

	namelen = sizeof(it);
	
	printf("nDigger pinging %s, send %d bytesn", 
		dst_host,
		IP_HEADSIZE + ICMP_HEADSIZE + sizeof(long)
		);
	
	seq = 1; /* first seq = 1 */
	signal(SIGINT, pstat); /* when press del or ctrl+c, call stat */

	/* to statistic the result , and then exit */
	signal(SIGALRM, ping); /* hook ping function to timer */

	alarm(iTimeOut); /* start timer, call ping every timeout */

	/* seconds */
	ping(SIGALRM);

	for (;;) {	/* waiting for every echo back */
			/* icmp packet and check it */
		register size;
		register u_char ttl;
		register delta;
		register iIpHeadLen;
	
		/* block to received echo back datagram */
	
		size = recvfrom(sock, buf, sizeof(buf), 0, 
		(struct sockaddr *)&it, &namelen);
		if (size == -1 && errno == EINTR) {
			/* receive error or system call */
			/* interrupted */
			continue;
		}
	
		/* calculate the round trip time, */
		/* time when receive minus time when send */
	
		delta = (int)((time_now() - lSendTime)/1000);
	
		/* get echo back packet and check its ip header */
	
		pIpHead = (IPHEAD *)buf;
	
		/* get the ip packet lenth */
		/* if too small, not the icmp echoreply packet */
		/* give it up */
	
		iIpHeadLen = (int)((pIpHead->ip_verlen & 0x0f) << 2);
		if (size < iIpHeadLen + ICMP_HEADSIZE) {
			continue;
		}

		ttl = pIpHead->ip_ttl; /* time to live param */
	
		/* get the icmp header information */
		pIcmpHead = (ICMPHEAD *)(buf + iIpHeadLen);
	
		/* not icmp echo reply packet, give it up */
		if (pIcmpHead->type != ICMP_ECHOREPLY) {
			continue;
		}
	
		/* not proper icmp sequent number, give it up */
		if (pIcmpHead->id != seq || pIcmpHead->seq != seq) {
			continue;
		}
	
		/* print out result for each icmp */
		/* echo reply information */
		sprintf( buf, "icmp_seq=%u bytes=%d ttl=%d", 
			pIcmpHead->seq, size, ttl );
		fprintf(stderr, "reply from %s: %s time=%d\n",
			host, buf, delta);
	
		/* calculate some statistic information */
		/* max, min, average round trip time */
		/* received icmp echo reply packet numbers */
		max = MAX(delta, max);
		min = min ? MIN(delta, min) : delta;
		total += delta;
		++ recvd;
	
		/* for next icmp sequence */
	
		++ seq;
	}
}

void ping(int signum) 
{
	char buf[200];
	int iPacketSize;

	/* make the icmp header information */

	ICMPHEAD *pIcmpHead = (ICMPHEAD *)buf;
	pIcmpHead->type = ICMP_ECHO;
	pIcmpHead->code = 0;
	pIcmpHead->id = seq;
	pIcmpHead->seq = seq;
	pIcmpHead->chksum = 0;

	/* store time information as icmp packet content, 4 bytes */
	/* u may store other information instead */

	*((long *)pIcmpHead->data) = time_now();

	iPacketSize = ICMP_HEADSIZE + 4; /* icmp packet length */

	/* icmp header check sum */

	pIcmpHead->chksum = ChkSum((u_short *)pIcmpHead, iPacketSize);

	/* remember the time when send for calculate round trip time */
	lSendTime = time_now();

	/* send the icmp packet to des host */
	if ( sendto(sock,
		buf, iPacketSize, 0, (struct sockaddr *)&it, sizeof(it) ) < 0) {
		perror("send failed");
		exit(6);
	}

	/* packet number been sent */
	++sent;

	/* reset the timer hooker to me again */
	alarm(iTimeOut);
}

/* print the statistic information for this time's ping */
void pstat(int signum) 
{
	if (sent) {
		printf(
			"\n---- %s ping statistics summerized by Digger----\n",
			host
			); 
		printf(
			"%d packets sent, %d packets received, %.2f%%\n",
			sent, recvd, (float)(sent-recvd)/(float)sent*100
		);
	}

	if (recvd) {
		printf(
			"round_trip min/avg/max: %d/%d/%d\n",
			min, total/recvd, max
		); 
	}

	exit(0);
} 
