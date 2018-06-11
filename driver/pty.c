#include <asm/system.h>
#include <cnix/errno.h>
#include <cnix/kernel.h>
#include <cnix/driver.h>
#include <cnix/list.h>
#include <cnix/string.h>

extern void put_queue(struct tty_struct * tty, unsigned short ch);

int pty_read(struct tty_struct * tty)
{
	int i, count;
	const char * data;
	struct tty_struct * btty; // buddy tty

	data = tty->t_userdata;
	count = tty->t_userdatacount;

	btty = GETSLA(tty);

	// XXX
	for(i = 0; i < count; i++)
		put_queue(btty, data[i]);

	return count;
}

int pty_write(struct tty_struct * tty)
{
	unsigned char ch;
	const char * data;
	int writed, count;
	struct tty_struct * btty; // buddy tty
	tty_queue_t * tq;
	unsigned long flags;

	if(MASTER(tty))
		return pty_read(tty);

	writed = 0;
	data = tty->t_userdata;
	count = tty->t_userdatacount;

	btty = GETMAS(tty);
	tq = &btty->t_tq;

	while(writed < count){
		ch = data[writed];
nl_proc:
		lockb_all(flags);

		if(EQ(tq->tq_count, MAX_INPUT)){
			if(EQ(btty->t_count, 0)){
				unlockb_all(flags);
				return writed;
			}

			current->sleep_spot = &btty->t_writeq_wait;
			sleepon(&btty->t_writeq_wait);

			if(anysig(current)){
				unlockb_all(flags);
				return -EINTR;
			}
		}

		unlockb_all(flags);

		tq->tq_buf[tq->tq_head] = ch;

		tq->tq_head += 1;
		if(tq->tq_head >= MAX_INPUT)
			tq->tq_head = 0;

		tq->tq_count += 1;

		wakeup(&btty->t_readq_wait);
		select_wakeup(&btty->t_select, OPTYPE_READ);

		if(ch == '\n'){
			if((tty->t_termios.c_oflag & OPOST)
				&& (tty->t_termios.c_oflag & ONLCR)){
				ch = '\r';
				goto nl_proc;
			}
		}

		writed += 1;
	}

	return writed;
}

extern void real_put_queue(
	struct tty_struct * tty, unsigned short ch, unsigned char attr
	);

void pty_echochar(struct tty_struct * tty, unsigned char ch)
{
	struct tty_struct * btty = pty_buddy(tty);

	real_put_queue(btty, ch, 0);
}

void pty_flush(struct tty_struct * tty)
{
	/* the process owing master pesudo-tty is running now */
}

int pty_getcol(struct tty_struct * tty)
{
	DIE("BUG: cannot happen\n");
	return 0;
}
