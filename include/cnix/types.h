/*
 * XXX: copy i586-cnix-pc/include/sys/types.h to test/include/sys/types.h,
 * and modify anything inconsistent with this file.
 */

#ifndef _TYPES_H
#define _TYPES_H

#ifdef NULL
#undef NULL
#endif

#define NULL	((void *)0)

#define NUL(x)	((x) == NULL)
#define ZERO(x)	((x) == 0)
#define EQ(a, b) ((a) == (b))	

typedef unsigned int	size_t;
typedef int		ssize_t;
typedef long		time_t;
typedef long		clock_t;
typedef long		suseconds_t;

typedef short		dev_t;

typedef short		uid_t;
typedef char		gid_t; /* XXX: 'short' in newlib */

typedef unsigned short	ino_t;
typedef unsigned short	mode_t; /* XXX: 'int' in newlib */ 
typedef char		nlink_t;

typedef long		off_t;

typedef unsigned long	zone_t;
typedef unsigned short	zone1_t;

typedef unsigned long	block_t;
typedef unsigned long	bit_t;

typedef int		pid_t;

typedef unsigned char	u8_t;
typedef unsigned short	u16_t;
typedef unsigned long	u32_t;

typedef char		i8_t;
typedef short		i16_t;
typedef long		i32_t;

typedef unsigned char * caddr_t;

typedef enum 
{
	FALSE,
	TRUE
} BOOLEAN;

enum 
{
	WAIT_TIMEOUT = 0,
	WAIT_OK
};

#define _SYS_TYPES_FD_SET
#define	NBBY	8		/* number of bits in a byte */

#ifndef	FD_SETSIZE
#define	FD_SETSIZE	64
#endif

typedef	long	fd_mask;

#define	NFDBITS	(sizeof (fd_mask) * NBBY)	/* bits per mask */
#ifndef	howmany
#define	howmany(x,y)	(((x)+((y)-1))/(y))
#endif

typedef	struct _types_fd_set 
{
	fd_mask	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} _types_fd_set;

#define fd_set _types_fd_set

#  define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1L << ((n) % NFDBITS)))
#  define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1L << ((n) % NFDBITS)))
#  define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1L << ((n) % NFDBITS)))
#  define	FD_ZERO(p)	(__extension__ (void)({ \
     size_t __i; \
     char *__tmp = (char *)p; \
     for (__i = 0; __i < sizeof (*(p)); ++__i) \
       *__tmp++ = 0; \
}))



#endif
