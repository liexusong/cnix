#ifndef _IOCTL_H
#define _IOCTL_H

#define FIONREAD	0x541B
#define FIONBIO		0x5421

#define TIOCPKT		0x5420
#define TIOCPKT_DATA		 0
#define TIOCPKT_FLUSHREAD	 1
#define TIOCPKT_FLUSHWRITE	 2
#define TIOCPKT_STOP		 4
#define TIOCPKT_START		 8
#define TIOCPKT_NOSTOP		16
#define TIOCPKT_DOSTOP		32

#endif
