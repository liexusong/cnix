#ifndef	_FCNTL_
#define _FCNTL_

#define O_ACCMODE	0003
#define O_RDONLY	00
#define O_WRONLY	01
#define O_RDWR		02
#define O_CREAT		0100
#define O_EXCL		0200
#define O_NOCTTY	0400
#define O_TRUNC		01000
#define O_APPEND	02000
#define O_NONBLOCK	04000
#define O_NDELAY	O_NONBLOCK
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

#define FD_CLOEXEC	0x01

#define R_OK	4
#define W_OK	2
#define X_OK	1
#define F_OK	0

extern int open(const char *, int flags, ...);

#endif
