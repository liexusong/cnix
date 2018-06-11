#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/timer.h>
#include <cnix/elf.h>
#include <cnix/fs.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>
#include <cnix/tty.h>
#include <cnix/socket.h>
#include <cnix/select.h>
#include <cnix/poll.h>

//#define SELECT_DEBUG 1

static BOOLEAN check_fd(int fd, int op)
{
	int flags;
	struct filp *p;
	
	if (ckfdlimit(fd))
	{
		return FALSE;
	}

	p = fget(fd);
	if (!p)
		return FALSE;

	flags = (p->f_mode & O_ACCMODE);
	
	return flags == O_RDWR || flags == op;
}

static BOOLEAN select_state(select_t * sel)
{
	if (sel->wait_spot /*&& *(sel->wait_spot)*/)
		return TRUE;

	return FALSE;
}

static void * get_type(int fd, int * type, BOOLEAN * selected)
{
	struct filp *p;
	struct inode *inoptr;
	unsigned short major;
	unsigned short minor;
	
	p = fget(fd);
	if (!p)
	{
		DIE("Bug: must have filp\n");
	}
	
	inoptr = p->f_ino;
	if (S_ISCHR(inoptr->i_mode))
	{
		major = MAJOR(inoptr->i_zone[0]);
		minor = MINOR(inoptr->i_zone[0]);

		if (major != 5 || minor > TTY_NUM)
		{
			return NULL;
		}

		*type = TYPE_TTY;
		*selected = select_state(&ttys[minor - 1].t_select);

		return &ttys[minor - 1];
	}
	else if (S_ISSOCK(inoptr->i_mode))
	{
		// XXX: may be we should record tty in this filed
		socket_t * sock = (socket_t *)inoptr->i_data;

		*type = TYPE_SOCK; 
		*selected = select_state(&sock->select);

		return sock;
	}
	else if (inoptr->i_flags & PIPE_I)
	{
		*type = TYPE_PIPE;
		*selected = select_state(&inoptr->i_select);

		return inoptr;
	}

	return NULL;
}

static BOOLEAN check_fd_read(int type, void * type_ptr)
{
	BOOLEAN ret = FALSE;
	struct inode * inoptr;

	if (!type_ptr)
		return TRUE;

	switch(type)
	{
	case TYPE_TTY:
		ret = tty_can_read(type_ptr);
		break;
	case TYPE_SOCK:
		ret = sock_can_read(type_ptr);
		break;
	case TYPE_PIPE:
		inoptr = type_ptr;
		if ((inoptr->i_size > 0) || !inoptr->i_buddy)
			return TRUE;
	
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;
	}

	return ret;
}

static BOOLEAN check_fd_write(int type, void * type_ptr)
{
	BOOLEAN ret = FALSE;
	struct inode * inoptr;

	if (!type_ptr)
		return TRUE;

	switch(type)
	{
	case TYPE_TTY:
		ret = tty_can_write(type_ptr);
		break;
	case TYPE_SOCK:
		ret = sock_can_write(type_ptr);
		break;
	case TYPE_PIPE:
		inoptr = type_ptr;
		if ((inoptr->i_size < 4096) || !inoptr->i_buddy)
			return TRUE;

		break;
	default:
		DIE("BUG: cannot happen\n");
		break;
	}

	return ret;
}

static BOOLEAN check_fd_except(int type, void * type_ptr)
{
	BOOLEAN ret = FALSE;

	if (!type_ptr)
		return TRUE;

	switch(type)
	{
	case TYPE_TTY:
		break;
	case TYPE_SOCK:
		ret = sock_can_except(type_ptr);
		break;
	case TYPE_PIPE:
		break;
	default:
		DIE("BUG: cannot happen\n");
		break;
	}

	return ret;
}

struct select_timeout
{
	int out;
	struct wait_queue ** spot;
};

static void select_timeout_callback(void * data)
{
	struct select_timeout * timeout = data;

	timeout->out = 1;
	wakeup(timeout->spot);
}

/* when return value is less than ZERO, some error occurs
 * is equal to ZERO timeout
 * is greater than ZER0 , ok
 */
static int select_wait_avail(
	struct wait_queue **wait, const struct timeval *val
	)
{
	unsigned long flags;
	synctimer_t sync;
	struct select_timeout timeout = { 0, wait};
	int btimer = 0;
	int err = 1;
	
	if (val)
	{
		sync.expire = val->tv_sec * HZ + (val->tv_usec * HZ) / 1000000;
		sync.data = &timeout;
		sync.timerproc = select_timeout_callback;
		synctimer_set(&sync);
		btimer = 1;
	}

	lockb_all(flags);

	current->sleep_spot = wait;
	sleepon(wait);
	
	if (timeout.out)
	{
		err = 0;	// the value 0 indicate timeout
	}
	else
	{
		if (anysig(current))
			err = -EINTR;

		if (btimer)
			sync_delete(&sync);
	}

	unlockb_all(flags);

	return err;
}

static short get_fd_optype(
	int fd, fd_set *readset, fd_set *writeset, fd_set *exceptset
	)
{
	short optype = 0;

	if (readset && FD_ISSET(fd, readset))
		optype |= OPTYPE_READ;

	if (writeset && FD_ISSET(fd, writeset))
		optype |= OPTYPE_WRITE;

	if (exceptset && FD_ISSET(fd, exceptset))
		optype |= OPTYPE_EXCEPT;

	return optype;
}

static int check_all_fds(int maxfdp1,
			fd_set *readset, 
			fd_set *writeset,
			fd_set *exceptset)
{
	int i;
	int count = 0;
	fd_set set1, set2, set3;

	FD_ZERO(&set1);
	FD_ZERO(&set2);
	FD_ZERO(&set3);

	for (i = 0; i < maxfdp1; i++)
	{
		short optype;
		int type = 0;
		void * type_ptr;
		BOOLEAN selected = FALSE;
		
		optype = get_fd_optype(i, readset, writeset, exceptset);
		if(!optype)
			continue;

		type_ptr = get_type(i, &type, &selected);
		if (selected)
		{
			// somebody has been doing select
			return -EAGAIN;
		}

		if (readset && FD_ISSET(i, readset))
		{
			if (!check_fd(i, O_RDONLY))
			{
				return -EBADF;
			}

			if (check_fd_read(type, type_ptr))
			{
				count++;
				FD_SET(i, &set1);
			}
		}

		if (writeset && FD_ISSET(i, writeset))
		{
			if (!check_fd(i, O_WRONLY))
			{
				return -EBADF;
			}

			if (check_fd_write(type, type_ptr))
			{
				count++;
				FD_SET(i, &set2);
			}
		}

		if (exceptset && FD_ISSET(i, exceptset))
		{
			if (check_fd_except(type, type_ptr))
			{
				count++;
				FD_SET(i, &set3);
			}
		}
	}

	if (count > 0)
	{
		if (readset)
			*readset = set1;

		if (writeset)
			*writeset = set2;

		if (exceptset)
			*exceptset = set3;
	}

	return count;
}

static struct fd_type *get_type_ptr(void *value,
		struct fd_type * start,
		struct fd_type * end)
{
	while (start < end)
	{
		if (start->type_ptr == value)
			return start;

		start++;
	}

	return NULL;
}

/*
 * 1. get one page
 * 2. initialize
 * 3. construct list
 * 4. add
 */
static struct fd_type * get_fd_list(
	int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset
	)
{
	int i;
	struct fd_type *ptr;
	struct fd_type *p;
	char *end;


	ptr = (struct fd_type *)get_one_page();
	if (!ptr)
	{
		//DIE("Bug: No free page?\n");
		return NULL;
	}
	p = ptr;

	end = (char *)ptr + PAGE_SIZE - sizeof(struct fd_type);

	for (i = 0; i < maxfdp1; i++)
	{
		short optype;
		int type = 0;
		void * type_ptr;
		BOOLEAN selected = FALSE;
		struct fd_type *old_ptr;

		optype = get_fd_optype(i, readset, writeset, exceptset);
		if (optype && (type_ptr = get_type(i, &type, &selected)))
		{
			/* the relation of fd map to ttyp  is N <==> 1 */
			if ((old_ptr = get_type_ptr(type_ptr, ptr, p)))
			{
				old_ptr->optype |= optype;
				continue;
			}
							
			if ((char *)p < end)
			{
				p->fd = i;
				p->type = type;
				p->optype = optype;
				p->type_ptr = type_ptr;
				p++;
			}
			else
			{
				printk("too many fd to select!\n");

				free_one_page((unsigned long)ptr);
				return NULL;
			}
		}
	}
	
	return ptr;
}

static void select_remove(select_t * sel)
{
	sel->optype = 0;
	sel->wait_spot = NULL;
}

static void remove_wait_spot(struct fd_type *head)
{
	unsigned long flags;
	
	lockb_all(flags);

	for (; head->type; head++)
	{
		if (head->type == TYPE_TTY)
		{
			struct tty_struct *ttyp;

			ttyp = head->type_ptr;
			select_remove(&ttyp->t_select);
		}
		else if (head->type == TYPE_SOCK)
		{
			socket_t * sock;

			sock = head->type_ptr;
			select_remove(&sock->select);
		}
		else if (head->type == TYPE_PIPE)
		{
			struct inode * inoptr;

			inoptr = head->type_ptr;
			select_remove(&inoptr->i_select);
		}
	}

	unlockb_all(flags);
}

static void select_add(
	select_t * sel, short optype, struct wait_queue ** spot
	)
{
	if (sel->wait_spot /*&& *(sel->wait_spot)*/)
		DIE("BUG: cannot happen\n");

	sel->optype = optype;
	sel->wait_spot = spot;
}

#ifdef SELECT_DEBUG
/* for debug */
static void print_fd_type(struct fd_type *t)
{
	if (t->type == TYPE_TTY)
	{
		printk("type: TYPE_TTY\t");
	}
	else if (t->type == TYPE_SOCK)
	{
		printk("type: TYPE_SOCK\t");
	}
	else if (t->type == TYPE_PIPE)
	{
		printk("type: TYPE_PIPE\t");
	}

	printk("op: ");
	if (t->optype & OPTYPE_READ)
	{
		printk("OPTYPE_READ, ");
	}
	if (t->optype & OPTYPE_WRITE)
	{
		printk("OPTYPE_WRITE, ");
	}
	if (t->optype & OPTYPE_EXCEPT)
	{
		printk("OPTYPE_EXCEPT");
	}
	printk("\t");

	printk("type ptr: 0x%08x\n", t->type_ptr);
}

static void print_all_fd_type(struct fd_type *head)
{
	unsigned long flags;

	lockb_all(flags);
	
	for (; head->type; head++)
	{
		print_fd_type(head);
	}

	unlockb_all(flags);
}
#endif

static void add_wait_spot(struct fd_type *head, struct wait_queue **spot)
{
	unsigned long flags;
	struct tty_struct *ttyp;
	socket_t * sock;
	struct inode * inoptr;

#ifdef SELECT_DEBUG
	print_all_fd_type(head);
#endif
	
	lockb_all(flags);

	for (; head->type; head++)
	{
		if (head->type == TYPE_TTY)	// add the spot of waiting.
		{
			ttyp = head->type_ptr;
			select_add(&ttyp->t_select, head->optype, spot);
		}
		else if (head->type == TYPE_SOCK)
		{
			sock = head->type_ptr;
			select_add(&sock->select, head->optype, spot);
		}
		else if (head->type == TYPE_PIPE)
		{
			inoptr = head->type_ptr;
			select_add(&inoptr->i_select, head->optype, spot);
		}
	}

	unlockb_all(flags);
}

int do_select(int maxfdp1,
		fd_set *readset, 
		fd_set *writeset, 
		fd_set *exceptset,
		struct timeval *timeout)
{
	int count;
	int err;
	struct fd_type *fd_ptr;
	struct wait_queue *tmp_wait = NULL;
	unsigned long flags;

	if (maxfdp1 < 0)
	{
		return -EINVAL;
	}

	if (timeout && (timeout->tv_sec < 0  || timeout->tv_usec < 0))
	{
		return -EINVAL;
	}

	lockb_all(flags);

	if ((count = check_all_fds(maxfdp1, readset, writeset, exceptset)) != 0)
	{
		unlockb_all(flags);
		return count;
	}

	// no wait
	if (timeout && timeout->tv_sec == 0 && timeout->tv_usec == 0)
	{
		/* reset the fdset */
		if (readset)
		{
			memset(readset, 0, sizeof(fd_set));
		}
		
		if (writeset)
		{
			memset(writeset, 0, sizeof(fd_set));
		}

		if (exceptset)
		{
			memset(exceptset, 0, sizeof(fd_set));
		}

		unlockb_all(flags);
		
		return 0;
	}

	// get the list of fd's for waiting
	fd_ptr = get_fd_list(maxfdp1, readset, writeset, exceptset);
	if (!fd_ptr)
	{
		unlockb_all(flags);
		return -EAGAIN;
	}

	add_wait_spot(fd_ptr, &tmp_wait);

	// Now we wait for something ok or timeout.
	err = select_wait_avail(&tmp_wait, timeout);

	unlockb_all(flags);

	remove_wait_spot(fd_ptr);

	if (err > 0)
	{
		// some fd ok
		err = check_all_fds(maxfdp1, readset, writeset, exceptset);
		if (err == 0)
		{
			DIE("Bug: no fd can read!\n");
			err = -EAGAIN;
		}
	}
	else if (err == 0)
	{
		/* reset the fdset */
		if (readset)
		{
			memset(readset, 0, sizeof(fd_set));
		}
		
		if (writeset)
		{
			memset(writeset, 0, sizeof(fd_set));
		}

		if (exceptset)
		{
			memset(exceptset, 0, sizeof(fd_set));
		}
	}
	
	free_one_page((unsigned long)fd_ptr);

	return err;
}

static int poll_all_fds(struct pollfd * fds, unsigned long nfds)
{
	int i;
	int count = 0;

	for(i = 0; i < nfds; i++)
	{
		int type = 0;
		void * type_ptr;
		BOOLEAN selected = FALSE;

		type_ptr = get_type(fds[i].fd, &type, &selected);
		if (selected)
		{
			// somebody has been doing select
			return -EAGAIN;
		}

		// don't care readable or not, this will be checked when reading
		if (fds[i].events | POLLIN)
		{
			if (check_fd_read(type, type_ptr))
			{
				fds[i].revents |= POLLIN;
				count++;
			}
		}

		if (fds[i].events | POLLOUT)
		{
			if (check_fd_write(type, type_ptr))
			{
				fds[i].revents |= POLLOUT;
				count++;
			}
		}

		if (fds[i].events | POLLPRI)
		{
			if (check_fd_except(type, type_ptr))
			{
				fds[i].revents |= POLLPRI;
				count++;
			}
		}
	}

	return count;
}

static struct fd_type * get_poll_list(struct pollfd * fds, unsigned long nfds)
{
	int i;
	struct fd_type *ptr;
	struct fd_type *p;
	char *end;

	ptr = (struct fd_type *)get_one_page();
	if (!ptr)
		return NULL;

	p = ptr;
	end = (char *)ptr + PAGE_SIZE - sizeof(struct fd_type);

	for(i = 0; i < nfds; i++)
	{
		if (fds[i].events & (POLLIN | POLLOUT))
		{
			int type;
			short optype;
			void * type_ptr;
			BOOLEAN selected = FALSE;

			type_ptr = get_type(i, &type, &selected);
			if (type_ptr)
			{
				if ((char *)p < end)
				{
					optype = 0;

					if(fds[i].events & POLLIN)
						optype |= OPTYPE_READ;

					if(fds[i].events & POLLOUT)
						optype |= OPTYPE_WRITE;

					if(fds[i].events & POLLPRI)
						optype |= OPTYPE_EXCEPT;

					p->fd = i;
					p->type = type;
					p->optype = optype;
					p->type_ptr = type_ptr;
					p++;
				}
				else
				{
					printk("too many fd to select!\n");

					free_one_page((unsigned long)ptr);
					return NULL;
				}
			}
		}
	}
	
	return ptr;
}

int do_poll(struct pollfd * fds, unsigned long nfds, int timeout)
{
	int count, err;
	struct fd_type *fd_ptr;
	struct wait_queue *tmp_wait = NULL;
	struct timeval tv;
	unsigned long flags;

	lockb_all(flags);

	if ((count = poll_all_fds(fds, nfds)))
	{
		unlockb_all(flags);
		return count;
	}

	if (!timeout)
	{
		unlockb_all(flags);
		return 0;
	}

	fd_ptr = get_poll_list(fds, nfds);
	if (!fd_ptr){
		unlockb_all(flags);
		return -EAGAIN;
	}

	// should lockb_all(flags) before this function is to be called?
	add_wait_spot(fd_ptr, &tmp_wait);

	if (timeout < 0)
	{
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	}
	else
	{
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = timeout * 1000;
	}

	// Now we wait for something ok or timeout.
	err = select_wait_avail(&tmp_wait, &tv);

	unlockb_all(flags);

	remove_wait_spot(fd_ptr);

	if (err == 0)
	{
		// some fd ok
		err = poll_all_fds(fds, nfds);
		if (err == 0)
			DIE("BUG: cannot happen\n"); // signal?
	}
	
	free_one_page((unsigned long)fd_ptr);

	return err;
}

void select_init(select_t * sel)
{
	sel->optype = 0;
	sel->wait_spot = NULL;
}

void select_wakeup(select_t * sel, short optype)
{
	if (sel->optype & optype)
	{
		wakeup(sel->wait_spot);
	}
}
