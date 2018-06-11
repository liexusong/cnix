#ifndef _TTY_H
#define _TTY_H

#include <cnix/vga.h>
#include <cnix/wait.h>
#include <cnix/types.h>
#include <cnix/time.h>
#include <cnix/termios.h>
#include <cnix/list.h>
#include <cnix/select.h>

#define PTY_NUM	(2 * 4)

#define TTY_NUM	(VGA_NUM + PTY_NUM)

#define MAX_CANON	255

/* the power of 2, mod(%) is replaced by head &(SIZE - 1) */
#define SIZE 		64 

typedef struct tty_queue_struct tty_queue_t;

struct tty_queue_struct{
	int tq_count;
	int tq_head, tq_tail;
	unsigned char tq_buf[MAX_INPUT];
	unsigned char tq_attr[MAX_INPUT]; /* low 4 bits for len in screen */
};

#define LENGTH(x)	((x) & 0x0f)

#define TTYVGA	0
#define PTYMAS	1
#define PTYSLA	2

#define ISVT(tty) (tty->t_type == TTYVGA)
#define ISPTY(tty) ((tty->t_type == PTYMAS) || (tty->t_type == PTYSLA))
#define MASTER(tty) (tty->t_type == PTYMAS)
#define SLAVE(tty) (tty->t_type == PTYSLA)

#define GETMAS(tty) (tty - 1)
#define GETSLA(tty) (tty + 1)

struct tty_struct{
	char t_type;
	char t_pktmode;
	tty_queue_t t_tq;
	const char * t_userdata;
	int t_userdatacount;
	char t_allow_read;
	struct wait_queue * t_read_wait;
	struct wait_queue * t_readq_wait;
	struct wait_queue * t_writeq_wait;
	select_t t_select;
	char t_allow_write, t_allow_write1;
	struct wait_queue * t_write_wait;
	struct vga_struct * t_vga;
	int t_escape;
	termios_t t_termios;
	unsigned long t_ccmap[4];
	pid_t t_owner;
	list_t t_session;
	unsigned long t_count;
	int (*t_read)(struct tty_struct *);
	int (*t_write)(struct tty_struct *);
	void (*t_echochar)(struct tty_struct *, unsigned char);
	void (*t_flush)(struct tty_struct *);
	int (*t_getcol)(struct tty_struct *);
};

extern struct tty_struct ttys[TTY_NUM];

extern BOOLEAN tty_can_read(struct tty_struct *ttyp);
extern BOOLEAN tty_can_write(struct tty_struct *ttyp);

extern struct tty_struct * pty_buddy(struct tty_struct * tty);

#define TCGETA		0x5405
#define TCSETA		0x5406
#define TCXONC		0x540A
#define TCFLSH		0x540B
#define TCSBRK		0x5425

/*
#define	TCOOFF		0
#define	TCOON		1
#define	TCIOFF		2
#define	TCION		3

#define	TCIFLUSH	0
#define	TCOFLUSH	1
#define	TCIOFLUSH	2
*/

#endif
