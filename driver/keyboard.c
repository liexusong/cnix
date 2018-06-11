#include <asm/io.h>
#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/driver.h>
#include <cnix/kernel.h>
#include <cnix/keymap.h>
#include <cnix/wait.h>
#include <cnix/signal.h>
#include <cnix/termios.h>
#include <cnix/vga.h>

#define SHIFTB	0x01
#define CTRLB	0x02
#define LALTB	0x04
#define RALTB	0x08

extern unsigned short * key_maps[];

static char esc = 0;
static unsigned char mode = 0;

/*
 * caps lock:   0x04
 * num lock:    0x02
 * scroll lock: 0x01
 */
unsigned char leds = 0;

static void save_code(unsigned char);

static unsigned char kbd_scan(void)
{
	unsigned char code;

	code = inb_p(0x60);
	outb_p((inb_p(0x61)) | 0x80, 0x61);
	outb_p((inb_p(0x61)) & 0x7f, 0x61);

	return code;
}

#define released(code)	((code) & 0x80)

void kbd_intr(struct regs_t * regs)
{
	unsigned short ch;
	unsigned char code;

	code = kbd_scan();
	if(EQ(code, 0) || EQ(code, 0xff))
		return;

	if(released(code)){
		ch = (key_maps[0])[code & 0x7f];
		if((ch != CTRL)
			&& (ch != SHIFT)
			&& (ch != ALT) 
			&& (ch != CLOCK)
			&& (ch != NLOCK)
			&& (ch != SLOCK)
			&& (ch != EXT))
			return;
	}

	save_code(code);

	raise_bottom(KEYBOARD_B);
}

static void kbd_wait(void)
{
	while(inb_p(0x64) & 0x2);
}

void setleds(void)
{
	kbd_wait();
	outb(0xed, 0x60);

	kbd_wait();
	outb(leds, 0x60);
}

void reboot()
{
	kbd_wait();
	outb(0xfc, 0x64);

	while(1);
}

#define KBDBUF_SIZE	32

typedef struct kbdbuf_struct kbdbuf_t;

struct kbdbuf_struct{
	int count;
	int head, tail;
	unsigned char buf[KBDBUF_SIZE];
}kbdbuf = {
	0, 0, 0,
};

kbdbuf_t * kbptr = &kbdbuf;

static void save_code(unsigned char code)
{
	kbptr->buf[kbptr->head] = code;
	kbptr->head++;
	kbptr->count++;

	if(EQ(kbptr->head, KBDBUF_SIZE))
		kbptr->head = 0;
}

extern struct tty_struct * cur_tty;

void kbd_bottom(void)
{
	if(NUL(cur_tty))
		DIE("cur_tty is NULL\n");

	cur_tty->t_read(cur_tty);
}

static unsigned short code2key(unsigned char code)
{
	unsigned short ch;
	unsigned short * map;
	int mapidx = mode;

	/* check ctrl + alt + del */
	if((mode & CTRLB) && (mode && (LALTB | RALTB)) && EQ(code, 83))
		return 0;

	ch = (key_maps[0])[code & 0x7f];
	if(EQ(esc, 0)){
		if((leds & 0x04) && (ch & CAPS))
			mapidx ^= SHIFTB;
		else if((leds & 0x02) 
			&& (((ch >= HOME) && (ch <= INSERT)) || EQ(ch, 0177)))
			mapidx ^= SHIFTB;
	}else{
		/* ignore everything but home->insert */
		if((ch < HOME) || (ch > INSERT)){
			/*
			 * without this if-statement, bug will show up
			 * when CTRL + W + ARROW_DOWN and release ARROW_DOWN 
			 * at last.
			 */
			if((ch != CTRL) && (ch != ALT)){
				esc = 0;
				return 0; 
			}
		}
	}

#define pressed(code)	(!((code) & 0x80))

	switch(ch){
	case CTRL: 
		if(pressed(code))
			mode |= CTRLB;
		else
			mode &= ~CTRLB;
		break;
	case SHIFT:
		if(pressed(code))
			mode |= SHIFTB;
		else
			mode &= ~SHIFTB;
		break;
	case ALT:
		if(pressed(code))
			mode |= (EQ(esc, 1) ? RALTB : LALTB);
		else
			mode &= ~(EQ(esc, 1) ? RALTB : LALTB);
		break;
	case CLOCK:
		if(pressed(code)){
			leds ^= 0x04;
			setleds();
		}
		break;
	case NLOCK:
		if(pressed(code)){
			leds ^= 0x02;
			setleds();
		}
		break;
	case SLOCK:
		if(pressed(code)){
			leds ^= 0x01;
			setleds();
		}
		break;
	case 0:		/* vmware */	
	case EXT:	/* bochs */
		esc = 1;
		break;
	default:
		if(NUL(map = key_maps[mapidx]))
			break;

		ch = map[code & 0x7f];

		return (ch & ~CAPS);
	}

	return 0;
}

static void print_process_info(struct task_struct * t)
{
	static char * state[] = {
		"running",
		"interruptible",
		"uninterruptible",
		"zombie",
		"stopped",
	};

	if(!t)
		return;

	printk(
		"%8d%8d%16x%24s   %s(%c)\n",
		t->pid,
		t->ppid,
		t->sleep_eip,
		EQ(t, current) ? "*running" : state[t->state],
		t->myname,
		(t->kernel ? 'k' : 'u')
		);
}

extern int nfreepages;
extern int inode_free_num;
extern void print_arp_table(void);
extern void print_route_table(void);
extern void print_netstat(void);

static void func_key(unsigned short ch)
{
	if(EQ(ch, F1)){
		int i;

		printk("\n");
		printk("     pid");
		printk("    ppid");
		printk("    sleeping eip");
		printk("                   state   name");
		printk("\n");
		for(i = 0; i < NR_TASKS; i++)
			print_process_info(task[i]);
		printk("     nfreepages: %d\n", nfreepages);
		printk("     inode_free_num: %d\n", inode_free_num);
	}
	else if (EQ(ch, F2))
	{
		print_arp_table();
	}
	else if (EQ(ch, F3))
	{
		print_route_table();
	}
	else if (EQ(ch, F4))
	{
		print_netstat();
	}
}

static int echo(struct tty_struct * tty, unsigned short ch)
{
	int len;
	termios_t * t = &tty->t_termios;

	if(!(t->c_lflag & ECHO)){
		if(EQ(ch, NL) && (CANON(t) && (t->c_lflag & ECHONL))){
			tty->t_echochar(tty, '\n');
			tty->t_flush(tty);
			return 0;
		}

		return 0;
	}

	if(ISPTY(tty)){
		tty->t_echochar(tty, ch);
		tty->t_flush(tty); // XXX
		return 1;
	}

	len = 1;

	if((ch < ' ') || (ch > 0xff)){
		if(EQ(ch, '\t')){
			int i;

			len = 8;
			len -= (tty->t_getcol(tty) & (8 - 1));

			for(i = 0; i < len; i++)
				tty->t_echochar(tty, ' ');
		}else if(EQ(ch, '\n') || EQ(ch, '\r') || EQ(ch, '\b')){
			len = 0;
			tty->t_echochar(tty, ch);
		}else if(tty->t_termios.c_lflag & ECHOCTL){
			char hi[] = "HYABDCVUGST@";

			len = 2;
			tty->t_echochar(tty, '^');

			if((ch >= HOME) && (ch <= INSERT))
				tty->t_echochar(tty, hi[ch - HOME]);
			else
				tty->t_echochar(tty, ch + '@');
		}else
			tty->t_echochar(tty, ch);
	}else if(EQ(ch, '\177')){
		len = 2;
		tty->t_echochar(tty, '^');
		tty->t_echochar(tty, '?');
	}else
		tty->t_echochar(tty, ch);

	tty->t_flush(tty);

	return len;
}

extern void beep_start(void);

void real_put_queue(
	struct tty_struct * tty,
	unsigned short ch,
	unsigned char attr)
{
	tty_queue_t * tq = &tty->t_tq;
	termios_t * t = &tty->t_termios;

	if(EQ(tq->tq_count, MAX_INPUT)){
		unsigned long flags;
		struct tty_struct * btty;

		if(ISVT(tty)){
			if(t->c_iflag & IMAXBEL)
				beep_start();

			return;
		}

		if(MASTER(tty))
			return;

		btty = pty_buddy(tty);

		lockb_all(flags);	

		if(EQ(btty->t_count, 0)){
			unlockb_all(flags);
			return;
		}

		current->sleep_spot = &tty->t_writeq_wait;
		sleepon(&tty->t_writeq_wait);

		if(anysig(current)){
			unlockb_all(flags);
			return;
		}

		unlockb_all(flags);
	}

	tq->tq_buf[tq->tq_head++] = ch & 0xff;
	tq->tq_attr[tq->tq_head] = attr; /* not used */

	if(EQ(tq->tq_head, MAX_INPUT))
		tq->tq_head = 0;

	tq->tq_count++;

	if(!CANON(t) || EQ(tq->tq_count, MAX_INPUT))
	{
		wakeup(&tty->t_readq_wait);
		select_wakeup(&tty->t_select, OPTYPE_READ);
	}
}

static void kbd_sig(struct tty_struct * tty, int signum)
{
	if(tty->t_owner > 0){
		pid_t pid = tty->t_owner;
		list_t * tmp, * pos;
		struct task_struct * t;

		foreach(tmp, pos, &task[pid]->pglist)
			t = list_entry(tmp, pglist, struct task_struct);
			sendsig(t, signum);
		endeach(&task[pid]->pglist);

		/* anyone who is waiting for tty */
		wakeup(&tty->t_read_wait);
	}
}

#define ECHAR(ch, t, IDX)	EQ((ch), (t)->c_cc[IDX])

static int sig_char(struct tty_struct * tty, unsigned char ch)
{
	int ret = 1;
	termios_t * t = &tty->t_termios;

	/* not support VDSUSP */

	if(ECHAR(ch, t, VINTR)) /* XXX: NOFLUSH */
		kbd_sig(tty, SIGINT);
	else if(ECHAR(ch, t, VQUIT)) /* XXX: NOFLUSH */
		kbd_sig(tty, SIGQUIT);
	else if(ECHAR(ch, t, VSUSP))
		kbd_sig(tty, SIGTSTP);
	else
		ret = 0;

	return ret;
}

static int exten_char(struct tty_struct * tty, unsigned char ch)
{
	int ret = 1;

	termios_t * t = &tty->t_termios;

	if(ECHAR(ch, t, VDISCARD)){

	}else if(ECHAR(ch, t, VLNEXT))
		tty->t_escape = 1;
	else
		ret = 0;

	return ret;
}

static int canon_char(struct tty_struct * tty, unsigned short ch)
{
	int ret = 1;
	termios_t * t = &tty->t_termios;

	if(EQ(ch, CR)){ 
		if(!(t->c_iflag & IGNCR)){
			if((t->c_iflag & ICRNL)){
				ch = NL;
				wakeup(&tty->t_readq_wait);
			}

			real_put_queue(tty, ch, echo(tty, ch));
		}
	}else if(EQ(ch, NL)){
		if(!(t->c_iflag & INLCR))
			wakeup(&tty->t_readq_wait);
		else
			ch = CR;

		real_put_queue(tty, ch, echo(tty, ch));
	}else if(ECHAR(ch, t, VEOF)){
		wakeup(&tty->t_readq_wait);
		real_put_queue(tty, ch, 1);
	}else if(ECHAR(ch, t, VEOL)){
		wakeup(&tty->t_readq_wait);
		real_put_queue(tty, ch, 1);
	}else if(ECHAR(ch, t, VEOL2))
		wakeup(&tty->t_readq_wait);
	else if((t->c_lflag & ECHOE) && ECHAR(ch, t, VERASE)){
		tty_queue_t * tq = &tty->t_tq;

		echo(tty, '\b');
		echo(tty, ' ');
		echo(tty, '\b');

		/* run under bottom half, need not to lock  */
		if(tq->tq_count > 0){
			tq->tq_count--;
			tq->tq_head--;
			if(tq->tq_head < 0)
				tq->tq_head = MAX_INPUT - 1;
		}
	}else if((t->c_lflag & ECHOK) && ECHAR(ch, t, VKILL)){
		echo(tty, '\n');
	}else if(ECHAR(ch, t, VREPRINT) && EXTEN(t)){
#if 0
	}else if(ECHAR(ch, t, VSTATUS)){
		/* NOKERNINFO */
	}else if(ECHAR(ch, t, VWERASE)){
		/* ALTWERASE(ECHOPRT) */
#endif
	}else
		ret = 0;

	return ret;
}

static int on_char(struct tty_struct * tty, unsigned char ch)
{
	int ret = 1;
	termios_t * t = &tty->t_termios;

	if(ECHAR(ch, t, VSTART)){
	 	if(t->c_iflag & IXON){
			tty->t_allow_write1 = 1;
			wakeup(&tty->t_write_wait);
		}else
			ret = 0;
	}else if(ECHAR(ch, t, VSTOP)){
		if(t->c_iflag & IXON)
			tty->t_allow_write1 = 0;
		else
			ret = 0;
	}else
		ret = 0;

	return ret;
}

static int is_special(struct tty_struct * tty, unsigned short ch)
{
	int i, j;

	if(ch & 0xff80)
		return 0;

	i = ch >> 5;
	j = ch & ((1 << 5) - 1);

	if(tty->t_ccmap[i] & (1 << j))
		return 1;

	return 0;
}

static BOOLEAN put_queue_for_vt(struct tty_struct * tty, unsigned short ch)
{
	termios_t * t = &tty->t_termios;

	if(ch > 0xff){ 
		if(EQ(ch, ALEFT)) 
			select_vga(-1);
		else if(EQ(ch, ARIGHT))
			select_vga(-2);
		else if((ch >= AF1) && (ch <= AF12))
			select_vga(ch - AF1);
		else if(EQ(ch, SPGUP)){
			vga_begin_page_up();
			return TRUE;
		}else if(EQ(ch, SPGDOWN)){
			vga_begin_page_down();
			return TRUE;
		}else if((ch >= HOME) && (ch <= INSERT)){
			char hi[] = "HYABDCVUGST@";

			if(t->c_lflag & ECHO)
				echo(tty, ch);

			real_put_queue(tty, '\033', 1);
			real_put_queue(tty, '\133', 1);
			real_put_queue(tty, hi[ch - HOME], 1);
		}
		else if ((ch >= F1) && (ch <= F12))
		{	/* No use for func_key */
			/* F1 \E[[A
			 * F2 \E[[B
			 * ..
			 * F12 \E[[L
			 */
			real_put_queue(tty, '\033', 1);
			real_put_queue(tty, '\133', 1);
			real_put_queue(tty, '\133', 1);
			real_put_queue(tty, 'A' + ch - F1, 1);
		}

		vga_end_page_up();

		return TRUE;
	}

	vga_end_page_up();

	return FALSE;
}

/*
 * see termios.h
 */
void put_queue(struct tty_struct * tty, unsigned short ch)
{
	termios_t * t = &tty->t_termios;

	if(ISVT(tty) && put_queue_for_vt(tty, ch))
		return;

	if(t->c_iflag & ISTRIP)
		ch &= 0x7f;

	if(t->c_iflag & IXANY)
		wakeup(&tty->t_write_wait);

	if(is_special(tty, ch)){
		if(!(tty->t_escape)){
			if(SIG(t) && sig_char(tty, ch))
				return;
			else if(CANON(t) && canon_char(tty, ch))
				return;
			else if(ONOFF(t) && on_char(tty, ch))
				return;
			else if(EXTEN(t) && exten_char(tty, ch))
				return;

			real_put_queue(tty, ch, echo(tty, ch));
	
			return;
		}
	}else if((t->c_iflag & IUCLC) && ((ch >= 'A') && (ch <= 'Z')))
		ch -= 'A' - 'a';

	tty->t_escape = 0;

	real_put_queue(tty, ch, echo(tty, ch));

	if(ISVT(tty)){
		if(EQ(ch, '\033')){ /* ESC key, escape it too */
			real_put_queue(tty, '\133', 1);
			real_put_queue(tty, '\033', 1);
		}
	}

#if 0
	if(t->c_lflag & PENDIN) { }
#endif
}

int kbd_read(struct tty_struct * tty)
{
	unsigned char code;
	unsigned short ch;
	unsigned long flags;

	lock(flags);

	while(kbptr->count > 0){
		code = kbptr->buf[kbptr->tail];

		kbptr->tail++;
		if(EQ(kbptr->tail, KBDBUF_SIZE))
			kbptr->tail = 0;

		kbptr->count--;

		unlock(flags);

		if(ZERO(mode)){
			ch = (key_maps[0])[code & 0x7f];			
			if((ch >= F1) && (ch <= F12)){
				func_key(ch);
				lock(flags);
				continue;
			}		
		}

		ch = code2key(code);
		if(ZERO(ch)){
			lock(flags);
			continue;
		}

		put_queue(tty, ch);

		lock(flags);
	}

	unlock(flags);

	return 0;
}

void kbd_init(void)
{
	setleds();
	kbd_scan();
	set_bottom(KEYBOARD_B, kbd_bottom);
	put_irq_handler(0x01, kbd_intr);
}
