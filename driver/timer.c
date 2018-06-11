#include <asm/io.h>
#include <asm/regs.h>
#include <asm/cmos.h>
#include <cnix/signal.h>
#include <asm/system.h>
#include <cnix/driver.h>
#include <cnix/kernel.h>

#define TIMER_MODE	0x43
#define TIMER0		0x40

#define TIMER_FREQ	1193182
#define LATCH		(TIMER_FREQ / HZ)

volatile unsigned long nowticks;
volatile unsigned long lost_ticks;
volatile unsigned long saved_lost_ticks;

static void timer_interrupt(struct regs_t * regs)
{
	struct task_struct * t = task[current->ppid];

	if(!(regs->cs & 0x3)){
		current->stime++;
		while(t && (t->pid != t->ppid)){
			t->cstime++;
			t = task[t->ppid];
		}
	}else{
		current->utime++;
		while(t && (t->pid != t->ppid)){
			t->cutime++;
			t = task[t->ppid];
		}
	}

	nowticks++;
	lost_ticks++;

	raise_bottom(TIMER_B);
}

static void do_with_sync0(void *);
static void do_with_sync1(void *);
static void do_with_async0(void *);
static void do_with_async1(void *);

static void timer_bottom(void)
{
	if(current != &idle_task_struct){
		if(current->counter > 0)
			current->counter--;
		else
			current->need_sched = 1;
	}else if(!list_empty(run_queue))
		current->need_sched = 1; /* force idle to give up cpu */

	do_with_sync0(NULL);
}

static synctimer_t sync_async0 = { 
	{ &sync_async0.list, &sync_async0.list },
	1,
	NULL,
	do_with_async0
};

static synctimer_t sync_async1 = {
	{ &sync_async1.list, &sync_async1.list },
	HZ,
	NULL,
	do_with_async1
};

static synctimer_t sync_sync1 = {
	{ &sync_sync1.list, &sync_sync1.list },
	HZ,
	NULL,
	do_with_sync1
};

list_t sync_list0 = { &sync_list0, &sync_list0 };
list_t sync_list1 = { &sync_list1, &sync_list1 };
list_t async_list0 = { &async_list0, &async_list0 };
list_t async_list1 = { &async_list1, &async_list1 };

static inline void sync_insert(synctimer_t * sync, i32_t which)
{
	list_t * sync_list;

	sync_list = ZERO(which) ? &sync_list0 : &sync_list1;
	list_add_head(sync_list, &sync->list);
}

inline void sync_delete(synctimer_t * sync)
{
	list_del1(&sync->list);
}

static inline void async_insert(asynctimer_t * async, i32_t which)
{
	list_t * async_list;

	async_list = ZERO(which) ? &async_list0 : &async_list1;
	list_add_head(async_list, &async->list);
}

inline void async_delete(asynctimer_t * async)
{
	list_del1(&async->list);
}

#define sync0_insert(sync)	sync_insert(sync, 0)
#define sync1_insert(sync)	sync_insert(sync, 1)

#define async0_insert(async)	async_insert(async, 0)
#define async1_insert(async)	async_insert(async, 1)

void synctimer_set(synctimer_t * sync)
{
	unsigned long flags;

	if(NUL(sync) || ZERO(sync->expire) || NUL(sync->timerproc))
		return;

	lockb_timer(flags);
	if(sync->expire > HZ)
		sync1_insert(sync);
	else
		sync0_insert(sync);
	unlockb_timer(flags);
}

void asynctimer_set(asynctimer_t * async)
{
	unsigned long flags;

	if(NUL(async) || ZERO(async->expire))
		return;

	lockb_timer(flags);
	if(async->expire > HZ)
		async1_insert(async);
	else
		async0_insert(async);
	unlockb_timer(flags);
}

static void do_with_sync0(void * data)
{
	unsigned long flags;
	list_t * tmp, * pos;
	synctimer_t * sync;

	lock(flags);
	saved_lost_ticks = lost_ticks;
	lost_ticks = 0;
	unlock(flags);

	list_foreach(tmp, pos, &sync_list0){
		sync = list_entry(tmp, list, synctimer_t);
		if(sync->expire > saved_lost_ticks)
			sync->expire -= saved_lost_ticks;
		else{
			sync->expire = 0;
			sync_delete(sync);
			sync->timerproc(sync->data);
		}
	}
}

static void do_with_sync1(void * data)
{
	list_t * tmp, * pos;
	synctimer_t * sync;

	list_foreach(tmp, pos, &sync_list1){
		sync = list_entry(tmp, list, synctimer_t);

		if(sync->expire > HZ)
			sync->expire -= HZ;

		if(sync->expire <= HZ){
			sync_delete(sync);
			sync0_insert(sync);
		}
	}

	sync_sync1.expire = HZ;
	sync0_insert(&sync_sync1);
}

static void do_with_async0(void * data)
{
	list_t * tmp, * pos;
	asynctimer_t * async;
	struct task_struct * sigproc;

	list_foreach(tmp, pos, &async_list0){
		async = list_entry(tmp, list, asynctimer_t);
		if(async->expire > saved_lost_ticks)
			async->expire -= saved_lost_ticks;
		else{
			async->expire = 0;
			async_delete(async);

			sigproc = list_entry(async, alarm, struct task_struct);
			sendsig(sigproc, SIGALRM);
		}
	}

	sync_async0.expire = 1;
	sync0_insert(&sync_async0);
}

static void do_with_async1(void * data)
{
	list_t * tmp, * pos;
	asynctimer_t * async;

	list_foreach(tmp, pos, &async_list1){
		async = list_entry(tmp, list, asynctimer_t);
		if(async->expire > HZ)
			async->expire -= HZ;

		if(async->expire <= HZ){
			async_delete(async);
			async0_insert(async);
		}
	}

	sync_async1.expire = HZ;
	sync0_insert(&sync_async1);
}

void mstart(mstate_t * ms)
{
	ms->prev = 0;
	ms->now = 0;
}

unsigned long melapsed(mstate_t * ms)
{
	unsigned long flags, count;

	lock(flags);
	outb(0x00, TIMER_MODE);
	count = inb(TIMER0);
	count |= inb(TIMER0) << 8;
	unlock(flags);
	
	/* count is decreasing, in two conditions count will be bigger than 
	 * ms->prev, 1 and 2 will set prev to appropriate value.
	 * 1: at first, ms->prev is 0
	 * 2: when accross 65535 to 0 
	 */
	ms->now += (count <= ms->prev ? (ms->prev - count) : 1);
	ms->prev = count;

	return (ms->now / (TIMER_FREQ / 1000));
}

void mdelay(unsigned long msec)
{
	mstate_t ms;

	mstart(&ms);
	while(melapsed(&ms) < msec);
}

typedef struct bios_time_struct{
	i32_t second;
	i32_t minute;
	i32_t hour;
	i32_t day;
	i32_t month;
	i32_t year;
	i32_t century;
}bios_time_t;

static bios_time_t bios_time; 
static unsigned long boot_time;

#define MINUTE	60
#define HOUR	(MINUTE * 60)
#define DAY	(HOUR * 24)
#define YEAR	(DAY * 365)

/* the months of leap year */
static i32_t month[12] = {
	0,
	DAY * (31),
	DAY * (31 + 29),
	DAY * (31 + 29 + 31),
	DAY * (31 + 29 + 31 + 30),
	DAY * (31 + 29 + 31 + 30 + 31),
	DAY * (31 + 29 + 31 + 30 + 31 + 30),
	DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31),
	DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31),
	DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30),
	DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31),
	DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30),
};

static unsigned long time_to_second(bios_time_t * time)
{
	unsigned long ret;
	i32_t year;

	/* this century is 1 less than what we offen think of */
	year = time->year + time->century * 100 - 1970;

	/* 1972 % 4 == 0, 1973 - 1970 == 3, 3 + 1 == 4 */
	ret = year * YEAR + DAY * ((year + 1) / 4);	

	ret += month[time->month - 1];
	/* if it's not leap year */
	if(time->month > 2 && ((year + 2) % 4))
		ret -= DAY;
	ret += DAY * (time->day - 1);
	ret += HOUR * time->hour;
	ret += MINUTE * time->minute;
	ret += time->second;

	return ret;
}

#if 0
#define leapyear(year) ((!(year % 400)) || (!(year % 4) && (year % 100)))

static void second_to_time(unsigned long seconds, bios_time_t * time)
{
	int year;
	unsigned long year_sec, secbak;

	for(year = 1970; ; year++){
		secbak = seconds;
		
		year_sec = YEAR;
		if (leapyear(year))
		{
			year_sec += DAY;
		}

		seconds -= year_sec;

		if(seconds > secbak){
			seconds += year_sec;
			break;
		}
	}

	time->year = year;

	{
		int i;

		if(leapyear(year))
			month[2] = DAY * (31 + 29);
		else
			month[2] = DAY * (31 + 28);
			
		for(i = 1; i < 12; i++)
			if(seconds < month[i])
				break;

		time->month = i;

		seconds -= month[i - 1];
		time->day = seconds / DAY + 1;

		seconds -= (time->day - 1) * DAY;
		time->hour = seconds / HOUR;

		seconds -= time->hour * HOUR;
		time->minute = seconds / MINUTE;

		seconds -= time->minute * MINUTE;
		time->second = seconds;
	}
}
#endif

#define UTC_OFFSET	8	// +8 hours beijing time
// the current time difference between current time and 00:00:00, utc
unsigned long utc_time_diff(void)
{
	int left;
	time_t cur;
	unsigned long ret;
	int hour;

	cur = curclock();
	left = (nowticks % HZ) * (1000 / HZ);

	ret = cur % (60 * 60 * 24);

	hour = ret / 3600;

	ret -= hour * 3600;
	
	hour -= UTC_OFFSET;
	if (hour < 0)
	{
		hour += 24;
	}

	ret += hour * 3600;
	ret *= 1000;
	ret += left;

	return ret;
}

time_t curclock(void)
{
	return (boot_time + nowticks / HZ);
}

void timer_init(void)
{
	bios_time.second = CMOS_READ(RTC_SECOND);
	bios_time.minute = CMOS_READ(RTC_MINUTE);
	bios_time.hour = CMOS_READ(RTC_HOUR);
	bios_time.day = CMOS_READ(RTC_DAY_OF_MONTH);
	bios_time.month = CMOS_READ(RTC_MONTH);
	bios_time.year = CMOS_READ(RTC_YEAR);
	bios_time.century = CMOS_READ(RTC_CENTURY);
	BCD_TO_BIN(bios_time.second);	BCD_TO_BIN(bios_time.minute);
	BCD_TO_BIN(bios_time.hour);	BCD_TO_BIN(bios_time.day);
	BCD_TO_BIN(bios_time.month);	BCD_TO_BIN(bios_time.year);
	BCD_TO_BIN(bios_time.century);

	boot_time = time_to_second(&bios_time);

	printk("Boot time: %u\n", boot_time);
	printk("Century %d Year %d Month %d Day %d Time: %d : %d : %d\n", 
		bios_time.century + 1,
		bios_time.year, bios_time.month,
		bios_time.day, bios_time.hour, 
		bios_time.minute, bios_time.second);

	nowticks = 0;
	lost_ticks = 0;

  	/* set timer rate */
	outb(0x36, TIMER_MODE);		/* binary, mode 2, LSB/MSB, ch 0 */
	outb(LATCH & 0xff, TIMER0);	/* LSB */
	outb(LATCH >> 8, TIMER0);	/* MSB */

	set_bottom(TIMER_B, timer_bottom);
	put_irq_handler(0x00, timer_interrupt);

	sync0_insert(&sync_async0);
	sync0_insert(&sync_async1);
	sync0_insert(&sync_sync1);
}
