#include <asm/system.h>
#include <cnix/errno.h>
#include <cnix/kernel.h>
#include <cnix/driver.h>
#include <cnix/list.h>
#include <cnix/ioctl.h>

struct tty_struct ttys[TTY_NUM];
struct tty_struct * cur_tty;

struct tty_struct * pty_buddy(struct tty_struct * tty)
{
	struct tty_struct * btty;

	if(MASTER(tty))
		btty = GETSLA(tty);
	else
		btty = GETMAS(tty);

	return btty;
}

static termios_t def_termios = {
	(BRKINT | ICRNL | IXON | IXANY),			/*c_iflag*/
	(OPOST | ONLCR),					/* c_oflag */
	(CREAD | CS8 | HUPCL),					/* c_cflag */
	(ISIG | IEXTEN | ICANON | ECHO | ECHOE | ECHOCTL),	/* c_lflag */
	0,							/* c_line */
	{							/* c_cc */
		DEF_INTR,
		DEF_QUIT,
		DEF_ERASE,
		DEF_KILL,
		DEF_EOF,
		DEF_TIME,
		DEF_MIN,
		DEF_STATUS, //XXXX////
		DEF_START,
		DEF_STOP,
		DEF_DSUSP,
		DEF_EOL,
		DEF_REPRINT,
		DEF_DISCARD, 
		DEF_WERASE,
		DEF_LNEXT,
		DEF_EOL2	
	},
	0xffffffff,
	0xffffffff
};

static void tty_field_init(struct tty_struct * tty)
{

	tty->t_pktmode		= 0;

	tty->t_tq.tq_count	= 0;
	tty->t_tq.tq_head 	= 0;
	tty->t_tq.tq_tail 	= 0;
	tty->t_userdata		= NULL;
	tty->t_userdatacount	= 0;

	tty->t_escape		= 0;
	tty->t_termios		= def_termios;
	tty->t_ccmap[0]		= 0x16fea518; /* compccmap */
	tty->t_ccmap[1]		= 0;
	tty->t_ccmap[2]		= 0;
	tty->t_ccmap[3]		= 0;
}

int tty_open(dev_t dev, int flags)
{
	struct tty_struct * tty;

	if((dev < 0) || (dev > TTY_NUM))
		return -ENXIO;

	if(!dev){
		if(current->tty != NOTTY)
			return 0;

		return -ENXIO;
	}

	tty = &ttys[dev - 1];

	if(ISPTY(tty)){
		if(MASTER(tty)){
			if(tty->t_count > 0)
				return -EBUSY;
		}else{
			struct tty_struct * btty = GETMAS(tty);

			if(EQ(btty->t_count, 0)) // master is not usable
				return -ENXIO;
		}

		if(EQ(tty->t_count, 0))
			tty_field_init(tty);
	}

	if(ZERO(tty->t_owner)
		&& !(flags & O_NOCTTY)
		&& (current->tty == NOTTY)){
		tty->t_owner = current->pid;
		current->tty = dev;
	}

	tty->t_count++;

	return 0;
}

int tty_close(dev_t dev, int flags)
{
	struct tty_struct * tty;

	if((dev < 0) || (dev > TTY_NUM))
		return -ENXIO;

	if(!dev)
		return 0;

	tty = &ttys[dev - 1];

	tty->t_count--;

	if(ISPTY(tty)){
		if(EQ(tty->t_count, 0)){
			struct tty_struct * btty = pty_buddy(tty);

			wakeup(&btty->t_readq_wait);
			wakeup(&btty->t_writeq_wait);

			select_wakeup(
				&btty->t_select, OPTYPE_READ | OPTYPE_WRITE
			);
		}
	}

	/* t_owner will be set to zero when the owner ends it's life */

	return 0;
}

struct tty_timeout{
	char out;
	struct wait_queue ** spot;
};

/*
 * in this kind of function(bottom half) shouldn't access any local variable
 * which is pointed by data.
 */
void tty_timeout_callback(void * data)
{
	struct tty_timeout * timeout = data;

	timeout->out = 1;
	wakeup(timeout->spot);
}

BOOLEAN tty_can_read(struct tty_struct *ttyp)
{
	tty_queue_t * tq;
	unsigned long flags;
	
	tq = &ttyp->t_tq;

	lockb_kbd(flags);

	if (!ZERO(tq->tq_count))
	{
		unlockb_kbd(flags);
		return TRUE;
	}

	unlockb_kbd(flags);

	if (ISPTY(ttyp))
	{
		struct tty_struct * btty = pty_buddy(ttyp);

		if (EQ(btty->t_count, 0))
			return TRUE;
	}

	return FALSE;
}

BOOLEAN tty_can_write(struct tty_struct * ttyp)
{	
	if (ISPTY(ttyp))
	{
		struct tty_struct * btty = pty_buddy(ttyp);

		if (btty->t_count > 0)
		{
			if (btty->t_tq.tq_count < MAX_INPUT)
				return TRUE;

			return FALSE;
		}
	}

	return TRUE;
}

ssize_t tty_read(
	dev_t dev,
	char * buffer,
	size_t count,
	off_t off,
	int * error,
	int openflags
	)
{
	unsigned char ch;
	unsigned long flags;
	unsigned char vmin, vtime;
	int readed, stop;
	termios_t * t;
	tty_queue_t * tq;
	struct tty_struct * tty;
	synctimer_t sync;
	int btimer = 0;
	struct tty_timeout timeout;

	if(!dev){
		if(current->tty == NOTTY)
			DIE("BUG: cannot happen\n");

		dev = current->tty;
	}

	tty = &ttys[dev - 1];

	if(current->tty != dev){
		if(ISVT(tty)){
			*error = -EINVAL;
			sendsig(current, SIGTTIN);
			return 0;
		}
	}

	timeout.out = 0;
	timeout.spot = &tty->t_readq_wait;

try_again:
	if(!tty->t_allow_read){
		sleepon(&tty->t_read_wait);

		if(anysig(current)){
			*error = -EINTR;
			return 0;
		}

		goto try_again;	
	}

	tty->t_allow_read = 0;

	/*
	 * how to do can avoid cause deadlock?
	 * if put the next lines code before try_again
	 */
	t = &tty->t_termios;
	vmin = t->c_cc[VMIN];
	vtime = t->c_cc[VTIME];	

	if(!CANON(t) && !vmin && (vtime > 0)){
		sync.expire = vtime * (HZ / 10);
		sync.data = &timeout;
		sync.timerproc = tty_timeout_callback;
		synctimer_set(&sync);
		btimer = 1;
	}

	readed = 0;
	stop = 0;
	tq = &tty->t_tq;

	// XXX
	if(MASTER(tty) && tty->t_pktmode){
		buffer[0] = TIOCPKT_DATA;
		readed = 1;
	}

	while((readed < count) && !stop){
		lockb_all(flags);

		if(ZERO(tq->tq_count)){
			if(!timeout.out){
				if(openflags & O_NDELAY){
					unlockb_all(flags);
					break;
				}

				if(ISPTY(tty)){
					struct tty_struct * btty;

					btty = pty_buddy(tty);

					if(EQ(btty->t_count, 0)){
						unlockb_all(flags);
						goto out;
					}
				}

				current->sleep_spot = &tty->t_readq_wait;
				sleepon(&tty->t_readq_wait);
			}

			if(timeout.out){
				unlockb_all(flags);
				break;
			}	

			if(anysig(current)){
				unlockb_all(flags);
				*error = -EINTR;
				break;
			}
		}	

		/* XXX: CANON check NL, or EOF, EOL?, EOL2?, VMIN, VTIME  */
		while((tq->tq_count > 0) && (readed < count)){
			ch = tq->tq_buf[tq->tq_tail];
			tq->tq_tail++;

			if(EQ(tq->tq_tail, MAX_INPUT))
				tq->tq_tail = 0;

			tq->tq_count--;

			buffer[readed] = ch;
			readed++;

			if(!CANON(t) && vmin){
				if(vtime){
					if((readed == 1) && (count != 1)){
						sync.expire = vtime * (HZ / 10);
						sync.data = &timeout;
						sync.timerproc = tty_timeout_callback;
						synctimer_set(&sync);
						btimer = 1;
					}
				}else{
					if(readed >= vmin){
						stop = 1;
						break;
					}
				}
			}else if(vtime){
				stop = 1;
				break;
			}

#define endline(c) \
	((c == NL) \
		|| (c == t->c_cc[VEOL]) \
		|| (c == t->c_cc[VEOL2]) \
		|| (c == t->c_cc[VEOF]))

			// XXX
			if(!MASTER(tty) && CANON(t) && endline(ch)){
				unlockb_all(flags);

				if(SLAVE(tty)){
					wakeup(&tty->t_writeq_wait);
					select_wakeup(
						&tty->t_select, OPTYPE_WRITE
					);
				}

				goto out;
			}
		}

		unlockb_all(flags);

		if(ISPTY(tty)){
			wakeup(&tty->t_writeq_wait);
			select_wakeup(&tty->t_select, OPTYPE_WRITE);

			if(MASTER(tty))
				break;
		}
	}

out:
	/* if there is a timer, delete it */
	if(btimer){
		lockb_timer(flags);
		/* 
		 * shouldn't use !list_empty(&sync.list) as condition,
		 * can cause dead lock, because list_del didn't set prev
		 * and next to itself.
		 */
		if(!timeout.out)
			list_del(&sync.list);
		unlockb_timer(flags);
	}

	tty->t_allow_read = 1;
	wakeup(&tty->t_read_wait);	

	return readed;
}

ssize_t tty_write(
	dev_t dev,
	const char * buffer,
	size_t count,
	off_t off,
	int * error,
	int openflags
	)
{
	int ret;
	struct tty_struct * tty;

	if(!dev){
		if(current->tty == NOTTY)
			DIE("BUG: cannot happen\n");

		dev = current->tty;
	}

	tty = &ttys[dev - 1];

	if(current->tty != dev){
		if(ISVT(tty)){
			*error = -EINVAL;
			sendsig(current, SIGTTIN);
			return 0;
		}
	}

try_again:
	if(!tty->t_allow_write || !tty->t_allow_write1){
		sleepon(&tty->t_write_wait);
		goto try_again;	
	}

	tty->t_allow_write = 0;

	/* TOSTOP(c_lflag), XCASE(c_lflag) */

	tty->t_userdata = buffer;
	tty->t_userdatacount = count;

	ret = tty->t_write(tty);

	tty->t_allow_write = 1;
	wakeup(&tty->t_write_wait);

	return ret;
}

static void compccmap(unsigned char cc[NCCS], unsigned long ccmap[4])
{
	int i;

	ccmap[0] = ccmap[1] = ccmap[2] = ccmap[3] = 0;

	for(i = 0; i < NCCS; i++){
		if((cc[i] > 127) || (i == VMIN) || (i == VTIME))
			continue;

		ccmap[cc[i] / 32] |= 1 << (cc[i] % 32);
	}

	ccmap[(unsigned char)CR / 32] |= 1 << ((unsigned char)CR % 32);
	ccmap[(unsigned char)NL / 32] |= 1 << ((unsigned char)NL % 32);
}

/*
 * caller has locked inode
 */
int tty_ioctl(dev_t dev, int request, void * data, int openflags)
{
	unsigned long flags;
	int ret = 0, param = 0;
	termios_t * termios = NULL;
	struct winsize * win = NULL;
	struct tty_struct * tty;

	if(!dev){
		if(current->tty == NOTTY)
			DIE("BUG: cannot happen\n");

		dev = current->tty;
	}

	tty = &ttys[dev - 1];

	if((request == TCGETA)
		|| (request == TCSAFLUSH)
		|| (request == TCSADRAIN)
		|| (request == TCSETA)
		|| (request == TCSANOW)){
		if(ckoverflow(data, sizeof(termios_t)))
			return -EFAULT;

		termios = (termios_t *)data;
	}else if((request == TCXONC)
		|| (request == TCFLSH)
		|| (request == TCSBRK)
		|| (request == TIOCPKT)){

		param = (int)data;
	}else if((request == TIOCGWINSZ) || (request == TIOCSWINSZ)){
		if(ckoverflow(data, sizeof(struct winsize)))
			return -EFAULT;

		win = (struct winsize *)data;
	}else if(request == TIOCPKT){
		if(ckoverflow(data, sizeof(int)))
			return -EFAULT;

		param = *(int *)data;
	}

	switch(request){
	case TCGETA:
		*termios = tty->t_termios;
		break;
	case TCSAFLUSH:
		lockb_kbd(flags);
		tty->t_tq.tq_count = 0;
		tty->t_tq.tq_head = tty->t_tq.tq_tail = 0;
		unlockb_kbd(flags);
	case TCSADRAIN:
		tty->t_flush(tty);
	case TCSETA: /* TCSANOW */
	case TCSANOW:
		tty->t_termios = *termios;
		compccmap(tty->t_termios.c_cc, tty->t_ccmap);
		break;
	case TCXONC:
		switch(param){
		case TCOOFF:
			tty->t_allow_write = 0;
			break;
		case TCOON:
			tty->t_allow_write = 1;
			break;
		case TCIOFF: // XXX
			tty->t_allow_read = 0;
			break;
		case TCION: // XXX
			tty->t_allow_read = 1;
			break;
		default:
			break;
		}
		break;
	case TCFLSH:
		switch(param){
		case TCIFLUSH:
			lockb_kbd(flags);
			tty->t_tq.tq_count = 0;
			tty->t_tq.tq_head = tty->t_tq.tq_tail = 0;
			unlockb_kbd(flags);
			break;
		case TCOFLUSH:
			tty->t_flush(tty); // XXX: should discard, but ...
			break;
		case TCIOFLUSH:
			lockb_kbd(flags);
			tty->t_tq.tq_count = 0;
			tty->t_tq.tq_head = tty->t_tq.tq_tail = 0;
			unlockb_kbd(flags);

			tty->t_flush(tty); // XXX: should discard, but ...
			break;
		default:
			break;
		}
		break;
	case TCSBRK: // don't care duration
		tty->t_flush(tty);
		break;
	case TIOCGWINSZ:
		win->ws_row = lines;
		win->ws_col = cols;
		win->ws_xpixel = 0;
		win->ws_ypixel = 0;
		break;
	case TIOCSWINSZ:
		if((win->ws_row != lines) || (win->ws_col != cols))
			return -EINVAL;
		break;
	case TIOCPKT:
		if(!MASTER(tty))
			return -EINVAL;

		tty->t_pktmode = param;

		break;
	default:
		printk("not support tty ioctl %x\n", request);
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

extern void kbd_init(void);
extern void vga_init(struct tty_struct *, int);
extern int vga_write(struct tty_struct *); /* already declared in tty.h */
extern void echochar(struct tty_struct *, unsigned char);
extern void vga_flush1(struct tty_struct *);
extern int vga_getcol(struct tty_struct *);
extern int kbd_read(struct tty_struct *);

int pty_read(struct tty_struct *);
int pty_write(struct tty_struct *);
void pty_echochar(struct tty_struct *, unsigned char);
void pty_flush(struct tty_struct *);
int pty_getcol(struct tty_struct *);

void tty_init(void)
{
	int i;
	struct tty_struct * tty;

	for(i = 0; i < TTY_NUM; i++){
		tty = &ttys[i];

		tty_field_init(tty);

		tty->t_owner			= 0;
		tty->t_count			= 0;

		tty->t_allow_read		= 1;
		wait_init(&tty->t_read_wait);

		tty->t_allow_write		= 1;
		tty->t_allow_write1		= 1;
		wait_init(&tty->t_write_wait);

		wait_init(&tty->t_readq_wait);
		wait_init(&tty->t_writeq_wait);

		select_init(&tty->t_select);

		if(i < VGA_NUM){
			tty->t_type		= TTYVGA;
			tty->t_read		= kbd_read;
			tty->t_write		= vga_write;
			tty->t_echochar		= echochar;
			tty->t_flush		= vga_flush1;
			tty->t_getcol		= vga_getcol;
		}else if(i < VGA_NUM + PTY_NUM){
			tty->t_type		=
				((i - VGA_NUM) % 2) ? PTYSLA : PTYMAS;
			tty->t_read		= pty_read;
			tty->t_write		= pty_write;
			tty->t_echochar		= pty_echochar;
			tty->t_flush		= pty_flush;
			tty->t_getcol		= pty_getcol;
		}

		vga_init(tty, i);
	}

	select_vga_init(0);

	kbd_init();	

	printk("Enabled displaying\n");
}
