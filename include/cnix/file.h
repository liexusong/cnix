#ifndef _FILE_H
#define _FILE_H

#include <cnix/types.h>
#include <cnix/inode.h>

#define NR_FILP		2048

struct filp{
	int	f_mode;
	int	f_count;
	struct inode * f_ino;
	off_t f_pos;
};

#define O_ACCMODE	0003
#define O_RDONLY	00
#define O_WRONLY	01
#define O_RDWR		02
#define O_CLOEXEC	04		// XXX: recorded in fflags
#define O_CREAT		0100
#define O_EXCL		0200
#define O_NOCTTY	0400
#define O_TRUNC		01000
#define O_APPEND	02000
#define O_NONBLOCK	04000
#define O_NDELAY	O_NONBLOCK	// XXX: recorded in fflags
#define O_SYNC		010000
#define O_FSYNC		O_SYNC
#define O_ASYNC		020000

/* the same value with linux */
#define F_DUPFD		0
#define F_GETFD		1
#define F_SETFD		2
#define F_GETFL		5
#define F_SETFL		6
#define F_GETOWN	9
#define F_SETOWN	8

#define R_OK	4
#define W_OK	2
#define X_OK	1
#define F_OK	0

extern struct filp * fget(int);
extern void ffree(struct filp *);
extern void fuse(struct filp *);
extern int fdget(int, mode_t, struct inode *, int *);

#endif
