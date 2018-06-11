#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/timer.h>
#include <cnix/elf.h>
#include <cnix/fs.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>
#include <cnix/stdarg.h>
#include <cnix/syslog.h>

/* Attention  */
/* don't call do_syslog from fs module & ide driver */

// for simpity. use root dir as parent dir
#define KLOG_PATH	"/dmesg"

static list_t log_queue;
static struct wait_queue *log_wait_spot;
static void ticks_to_hms(int *hour, int *min, int *sec, int *ms);

typedef struct log_req_struct
{
	list_t list;
	int buff_len;
	char *buff;
} log_req_t;

static int prepare_fs(void)
{
	int fd;
		
	nochk();
	fd = do_open(KLOG_PATH, O_RDWR | O_APPEND | O_CREAT, 0600);
	rechk();

	return fd;
}

/* It is too expensive to open and close log file ,
 * but I have no good solution. */
void klogd(void *data)
{
	list_t *tmp;
	log_req_t *req;
	int fd = -1;
	int ret;
	
	log_wait_spot = NULL;
	list_init(&log_queue);

	strcpy(current->myname, "klogd");

	for (;;)
	{
		if (list_empty(&log_queue))
		{
			if (fd >= 0)
			{
				do_close(fd);
				fd = -1;
			}
			
			sleepon(&log_wait_spot);
			continue;
		}
		
		tmp = log_queue.next;
		list_del(tmp);
		
		req = list_entry(tmp, list, log_req_t);

		if (fd < 0)
		{
			fd = prepare_fs();
		}
		
		if (fd < 0)
		{
			goto done;
		}
	
		ret = do_write(fd, req->buff, req->buff_len);
		
done:
		free_one_page((unsigned long)req);
	}
}


/* called from kernel, 
assume the message's length is less than PAGESIZE - sizeof(log_req_t) - 5 */
int do_syslog(int priority, const char *fmt, ...)
{
	va_list ap;
	int len;
	log_req_t *r;
	int h, m, s, ms;

	if (!(r = (log_req_t *)get_one_page()))
	{
		return -1;
	}

	list_init(&r->list);
	r->buff = (char *)r + sizeof(log_req_t);

	ticks_to_hms(&h, &m, &s, &ms);
	sprintf(r->buff, "<%d> %d:%d:%d.%d ", 
		priority & LOG_PRIMASK,
		h, m, s, ms);
	len = strlen(r->buff);
	
	va_start(ap, fmt);
	len += vsprintf(r->buff + len, fmt, ap);	// XXX: the %m
	va_end(ap);
	
	r->buff[len++] = '\n';
	r->buff[len] = '\0';
	r->buff_len = len;

//	printk("[%s]", r->buff);

	list_add_tail(&log_queue, &r->list);
	wakeup(&log_wait_spot);

	return len;
}

/* called from user program */
int do_user_syslog(int priority, const char *buff, int len)
{
	log_req_t *r;
	int n;
	int h, m, s, ms;

	if (cklimit(buff) || ckoverflow(buff, len))
	{
		return -EINVAL;
	}

	if (len > PAGE_SIZE - sizeof(log_req_t))
	{
		return -EINVAL;
	}

	if (!(r = (log_req_t *)get_one_page()))
	{
		return -EAGAIN;
	}

	list_init(&r->list);
	r->buff = (char *)r + sizeof(log_req_t);

	ticks_to_hms(&h, &m, &s, &ms);
	n = sprintf(r->buff, "<%d> %d:%d:%d.%d ", 
		priority & LOG_PRIMASK,
		h, m, s, ms);

	memcpy(r->buff + n, buff, len);
	r->buff_len = len + n;

	list_add_tail(&log_queue, &r->list);
	wakeup(&log_wait_spot);

	return 0;
}

static void ticks_to_hms(int *hour, int *min, int *sec, int *ms)
{
	int s;

	s = nowticks / HZ;

	*hour = s / 3600;
	*min = (s % 3600) / 60;
	*sec = s % 60;
	*ms = (nowticks % HZ) * (1000 / HZ);
}
