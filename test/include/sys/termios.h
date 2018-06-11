#ifndef _TERMIOS_H
#define _TERMIOS_H

#define MAX_INPUT	255

/* c_iflag */
#define BRKINT	(1 << 0)
#define ICRNL	(1 << 1)
#define IGNBRK	(1 << 2)
#define IGNCR	(1 << 3)
#define IGNPAR	(1 << 4)
#define IMAXBEL	(1 << 5)
#define INLCR	(1 << 6)
#define INPCK	(1 << 7)
#define ISTRIP	(1 << 8)
#define IUCLC	(1 << 9)
#define IXANY	(1 << 10)
#define IXOFF	(1 << 11)
#define IXON	(1 << 12)
#define PARMRK	(1 << 13)

/* c_oflag, ignore */
#define	BSDLY	(1 << 0)
#define CRDLY	(1 << 1)
#define FFDLY	(1 << 2)
#define NLDLY	(1 << 3)
#define OCRNL	(1 << 4)
#define OFDEL	(1 << 5)
#define OFILL   (1 << 6)
#define OLCUC   (1 << 7)
#define ONLCR   (1 << 8)
#define ONLRET  (1 << 9)
#define ONOCR   (1 << 10)
#define ONOEOT  (1 << 11)
#define OPOST   (1 << 12)
#define OXTABS	(1 << 13)
#define TABDLY	(1 << 14)
#define VTDLY	(1 << 15)

/* c_cflag */
/*
#define CCTS_OFLOW	(1 << 0)
#define CIGNORE         (1 << 1)
#define CLOCAL          (1 << 2)
#define CREAD           (1 << 3)
#define CRTS_IFLOW      (1 << 4)
#define CSIZE           ((1 << 5) || (1 <<6))
#define CS5		(0)
#define CS6		(1 << 5)
#define CS7		(1 << 6)
#define CS8		((1 << 5) || (1 << 6))
#define CSTOPS          (1 << 7)
#define HUPCL           (1 << 8)
#define MDMBUF          (1 << 9)
#define PARENB          (1 << 10)
#define PARODD          (1 << 11)
*/

/* XXX */
#define CBAUD	0010017
#define B0	0000000		/* hang up */
#define B50	0000001
#define B75	0000002
#define B110	0000003
#define B134	0000004
#define B150	0000005
#define B200	0000006
#define B300	0000007
#define B600	0000010
#define B1200	0000011
#define B1800	0000012
#define B2400	0000013
#define B4800	0000014
#define B9600	0000015
#define B19200	0000016
#define B38400	0000017
#define EXTA	B19200
#define EXTB	B38400
#define CSIZE	0000060
#define CS5	0000000
#define CS6	0000020
#define CS7	0000040
#define CS8	0000060
#define CSTOPB	0000100
#define CREAD	0000200
#define PARENB	0000400
#define PARODD	0001000
#define HUPCL	0002000
#define CLOCAL	0004000
#define CBAUDEX 0010000
#define B57600	0010001
#define B115200	0010002
#define B230400 0010003
#define B460800 0010004
#define B500000 0010005
#define B576000 0010006
#define B921600 0010007
#define B1000000 0010010
#define B1152000 0010011
#define B1500000 0010012
#define B2000000 0010013
#define B2500000 0010014
#define B3000000 0010015
#define B3500000 0010016
#define B4000000 0010017
#define CIBAUD	 002003600000	/* input baud rate (not used) */
#define CMSPAR	 010000000000	/* mark or space (stick) parity */
#define CRTSCTS	 020000000000	/* flow control */

/* c_lflag */
#define ALTWERASE	(1 << 0)
#define ECHO            (1 << 1)
#define ECHOCTL         (1 << 2)
#define ECHOE           (1 << 3)
#define ECHOK           (1 << 4)
#define ECHOKE          (1 << 5)
#define ECHONL          (1 << 6)
#define ICHOPRT		(1 << 7)
#define FLUSHO          (1 << 8)
#define ICANON          (1 << 9)
#define IEXTEN          (1 << 10)
#define ISIG            (1 << 11)
#define NOFLSH          (1 << 12)
#define NOKERNINFO      (1 << 13)
#define PENDIN		(1 << 14)
#define TOSTOP		(1 << 15)
#define XCASE		(1 << 16)

//#define CANON(t)	((t)->c_lflag & ICANON)
//#define SIG(t)		((t)->c_lflag & ISIG)
//#define EXTEN(t)	((t)->c_lflag & IEXTEN)
//#define ONOFF(t)	((t)->c_lflag & (IXON | IXOFF))

enum{
	VDISCARD, 
	VDSUSP, 
	VEOF, 
	VEOL, 
	VEOL2, 
	VERASE, 
	VINTR, 
	VKILL, 
	VLNEXT,
	VQUIT,
	VREPRINT,
	VSTART,
	VSTATUS,
	VSTOP,
	VSUSP,
	VWERASE,
	VMIN,
	VTIME,
	NCCS,
};

#if 0
#define	CR		'\r'
#define	NL		'\n'
#define	DEF_DISCARD 	'\17'	/* ^O */
#define	DEF_DSUSP	'\31'	/* ^Y */
#define	DEF_EOF 	'\4'	/* ^D */
#define	DEF_EOL 	0xff
#define	DEF_EOL2 	0xff
#define	DEF_ERASE	'\10'	/* ^H */
#define	DEF_INTR 	'\3'	/* ^C */
#define	DEF_KILL 	'\25'	/* ^U */
#define	DEF_LNEXT	'\26'	/* ^V */
#define	DEF_QUIT	'\34'	/* ^\ */
#define	DEF_REPRINT	'\22'	/* ^R */
#define	DEF_START	'\21'	/* ^Q */
#define	DEF_STATUS	'\24'	/* ^T */
#define	DEF_STOP	'\23'	/* ^S */
#define	DEF_SUSP	'\32'	/* ^Z */
#define	DEF_WERASE	'\27'	/* ^W */
#define	DEF_MIN		0
#define	DEF_TIME	0
#endif

typedef unsigned char	cc_t;
typedef unsigned int	speed_t;
typedef unsigned int	tcflag_t;

typedef struct termios termios_t;

struct termios{
	tcflag_t c_iflag;	
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_cc[NCCS];
	speed_t c_ispeed;
	speed_t c_ospeed;
};

#define TCGETA		0x5405
#define TCSETA		0x5406
#define TCSANOW		TCSETA
#define TCSADRAIN	0x5407
#define TCSAFLUSH	0x5408
#define TCXONC		0x540A
#define TCFLSH		0x540B
#define TCSBRK		0x5425

#define	TCOOFF		0
#define	TCOON		1
#define	TCIOFF		2
#define	TCION		3

#define	TCIFLUSH	0
#define	TCOFLUSH	1
#define	TCIOFLUSH	2

int tcgetattr(int fd, struct termios * t);
int tcsetattr(int fd, int request, const struct termios * t);

#endif
