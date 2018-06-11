#include <asm/io.h>
#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/wait.h>
#include <cnix/partition.h>
#include <cnix/fs.h>
#include <cnix/kernel.h>
#include <cnix/driver.h>
#include <cnix/list.h>
#include <cnix/string.h>

#define ide_io_read(port,buf,nr) \
	do{ \
		long _v; \
		__asm__ volatile( \
			"cld;rep;insw" \
			:"=D"(_v) \
			:"d"(port),"0"(buf),"c"(nr) \
			); \
	}while(0)

#define ide_io_write(port,buf,nr) \
	do{ \
		long _v; \
		__asm__ volatile( \
			"cld;rep;outsw" \
			:"=S"(_v) \
			:"d"(port),"0"(buf),"c"(nr) \
			); \
	}while(0)

static unsigned int cyl;
static unsigned char head;
static unsigned int sec;
static unsigned int precomp;

#define MAX_ERRS	3
static int need_reset = 0, errs = 0;

static struct dev_info{
	int flag;
	unsigned int start_sec;
	unsigned int num_sec;
}dev_info[5];
		
static struct request{
	int sec;
	int nsec;
	int rw_mode;
	list_t list;
	struct buf_head * buf;
}reqq[REQQ_SIZE];

static struct request * cur_req = NULL;
static list_t req_freelist;
static struct wait_queue * req_wait = NULL;

int dma_ide_flag = 0;
unsigned short dma_base_addr;

#define DMA_COMMAND_REG		0
#define DMA_STATUS_REG		2
#define DMA_PRD_ADDR_REG	4

/* physical_region_descriptor */
struct ide_prd{ 
	unsigned long addr;
	unsigned long len; // EOT ... len, last one if EOT is 1
}ide_prd;

unsigned char ide_prdbuff[1024];

static int ide_waitfor(int, int);
static int finish_cmd(void);
static int ide_reset(void);
static void ide_status_ok(void);
static int free_cur_req(void);
static void ide_intr(struct regs_t *);
static int ide_cmd(int, int, unsigned int, unsigned int);
static int dma_ide_cmd(int, int, unsigned int, unsigned int);
static struct request * get_req(void);
static void get_ide_info(void);

static int (*ide_cmd_func)(int, int, unsigned int, unsigned int);

void ide_init(void);

static int ide_waitfor(int mask, int value)
{
	long flags;
	mstate_t ms;

	lock(flags);

	mstart(&ms);
	do{
		if((inb(IDE_STATUS) & mask) == value){
			unlock(flags);
			return 1;
		}
	}while(melapsed(&ms) < TIMEOUT);

	need_reset = 1;

	unlock(flags);

	return 0;
}

static int ide_reset(void)
{
	long flags;

	lock(flags);

	mdelay(500);

	outb(RESET_CTL, IDE_CTL);
	mdelay(1);
	outb(0, IDE_CTL);
	mdelay(1);

	unlock(flags);

	if(!ide_waitfor(BUSY_STAT | READY_STAT, READY_STAT))
		return 0;
	
	lock(flags);

	need_reset = 0;

	unlock(flags);

	return 1;
}

static void ide_status_ok(void)
{
	long flags;

	lock(flags);

	while(inb(IDE_STATUS) & BUSY_STAT)
		mdelay(50);

	unlock(flags);
}

static int free_cur_req(void)
{
	struct request * tmp;

	if(list_empty(&req_freelist))
		wakeup(&req_wait);

	tmp = cur_req;

	if(list_empty(&cur_req->list)){
		cur_req = NULL;

		list_add_head(&req_freelist, &tmp->list);
		return 0; /* nothing left */
	}

	cur_req = list_entry(cur_req->list.next, list, struct request);

	list_del1(&tmp->list);
	list_add_head(&req_freelist, &tmp->list);

	return 1; /* have some work to do */
}

void ide_intr(struct regs_t * regs)
{
	raise_bottom(IDE_B);	
}

void ide_bottom(void)
{
	long flags;

	if(!cur_req){
		static int first = 0;

		if(!first){
			first = 1;
			return;
		}

		DIE("cur_req is NULL!!!\n");
	}

	if(dma_ide_flag){
		lock(flags);

		// clear interrupt
		outb(0x04, dma_base_addr + DMA_STATUS_REG);
		inb(dma_base_addr + DMA_STATUS_REG);

		// stop
		outb(0x00, dma_base_addr + DMA_COMMAND_REG);

		unlock(flags);

		if(cur_req->rw_mode == READ)
			memcpy(cur_req->buf->b_data, ide_prdbuff, 1024);

		cur_req->nsec = 0;
	}else{
		unsigned char * data = cur_req->buf->b_data;

		lock(flags);

		while(inb(IDE_STATUS) & BUSY_STAT)
			printk("looping\n");

		if(cur_req->rw_mode == READ){
			int offset = 0;

			if(cur_req->nsec < 2)
				offset = 512;

			ide_io_read(IDE_DATA, &data[offset], 256);
		}else{ // WRITE 
			if(cur_req->nsec > 1)
				ide_io_write(IDE_DATA, &data[512], 256);
		}

		unlock(flags);

		cur_req->nsec--;
	}

	/* need to see if it is the end, and reset hard disk */
	if(cur_req->nsec == 0){
		iodone(cur_req->buf);

		if(!free_cur_req())
			return;

		finish_cmd();
	}
}

static int ide_cmd0(int drive, int cmd, unsigned int sec, unsigned int nsec)
{
	int c, h, s;

	s = sec & 0xff;
	c = (sec & 0xffff00) >> 8;
	h = (sec & 0xf000000) >> 24;

	if((inb(IDE_STATUS) & BUSY_STAT) != 0 || !ide_waitfor(BUSY_STAT, 0)){
		printk("ide controller is not ready!\n");
		return 0;
	}
	
	outb(0xe0 | (drive << 4) | h, IDE_CURRENT);

	if((inb(IDE_STATUS) & BUSY_STAT) != 0 || !ide_waitfor(BUSY_STAT, 0)){
		printk("ide controller is not ready!\n");
		return 0;
	}
	
	outb(head >= 8 ? 8 : 0, IDE_CTL);
	outb(precomp, IDE_PRECOMP);

     	outb(nsec, IDE_NSECTOR);
	outb(s, IDE_SECTOR);
	outb(c, IDE_LCYL);
	outb(c >> 8, IDE_HCYL);
	
	outb(cmd, IDE_CMD);

	return 1;
}

static int ide_cmd(int drive, int cmd, unsigned int sec, unsigned int nsec)
{
	int ret;
	unsigned long flags;

	lock(flags);

	ret = ide_cmd0(drive, cmd, sec, nsec);

	if(cmd == WRITE_CMD){
		while((inb(IDE_STATUS) & DRQ_STAT) != DRQ_STAT)
			printk("looping\n");
	}

	unlock(flags);

	return ret;
}

static int dma_ide_cmd(int drive, int cmd, unsigned int sec, unsigned int nsec)
{
	unsigned char dmacmd;
	unsigned long flags;

	lock(flags);

	/* bit2: r/w, bit1: reserved, bit0: start/stop */
	outb(0x00, dma_base_addr + DMA_COMMAND_REG);

	/* clear interrupt and error */
	outb(0x06, dma_base_addr + DMA_STATUS_REG); 

	ide_prd.addr = (unsigned long)ide_prdbuff;
	ide_prd.len = (512 * nsec) | 0x80000000; // EOT = 1

	outl((unsigned long)&ide_prd, dma_base_addr + DMA_PRD_ADDR_REG);

	dmacmd = (cmd == DMA_READ_CMD) ? 0x08 : 0; 
	outb(dmacmd, dma_base_addr + DMA_COMMAND_REG);

	if(!ide_cmd0(drive, cmd, sec, nsec)){
		// clear interrupt
		outb(0x04, dma_base_addr + DMA_STATUS_REG);
		inb(dma_base_addr + DMA_STATUS_REG);

		// stop
		outb(0x00, dma_base_addr + DMA_COMMAND_REG);

		unlock(flags);

		return 0;
	}

	if(cmd == DMA_WRITE_CMD){
		while((inb(IDE_STATUS) & DRQ_STAT) != DRQ_STAT)
			printk("looping\n");
	}

	inb(dma_base_addr + DMA_COMMAND_REG);
	inb(dma_base_addr + DMA_STATUS_REG);

	outb(dmacmd | 0x01, dma_base_addr + DMA_COMMAND_REG);

	unlock(flags);

	return 1;
}

static int finish_cmd(void)
{
	unsigned char cmd;

	if(!cur_req)
		DIE("cur_req is NULL!!!\n");

	if(cur_req->rw_mode == READ){
		cmd = READ_CMD;
		if(dma_ide_flag)
			cmd = DMA_READ_CMD;

		if(!ide_cmd_func(0, cmd, cur_req->sec, cur_req->nsec)){
			errs++;
			while(errs < MAX_ERRS){
				if(need_reset)
					if(!ide_reset())
						DIE("ide_reset is useless!\n");
				if(ide_cmd_func(0, cmd, 
					cur_req->sec, cur_req->nsec))

					break;

				errs++;
			}

			if(errs == MAX_ERRS){
				printk("So many times for error!\n");
				cur_req->buf->b_flags |= B_ERROR;

				return 0;	
			}

			errs = 0;
		}
	}else if(cur_req->rw_mode == WRITE){
		cmd = WRITE_CMD;

		if(dma_ide_flag){
			cmd = DMA_WRITE_CMD;
			memcpy(ide_prdbuff, cur_req->buf->b_data, 1024);
		}

		if(!ide_cmd_func(0, cmd, cur_req->sec, cur_req->nsec)){
			errs++;
			while(errs < MAX_ERRS){
				if(need_reset)
					if(!ide_reset())
						DIE("ide_reset is useless!\n");
				if(ide_cmd_func(0, cmd, 
					cur_req->sec, cur_req->nsec))
					break;

				errs++;
			}

			if(errs == MAX_ERRS){
				printk("So many times for error!\n");
				cur_req->buf->b_flags |= B_ERROR;

				return 0;	
			}

			errs = 0;
		}

		if(!dma_ide_flag){
			unsigned long flags;

			lock(flags);
			ide_io_write(IDE_DATA, cur_req->buf->b_data, 256);
			unlock(flags);
		}
	}else
		DIE("rw_mode %d out of value...\n", cur_req->rw_mode);

	return 1;	
}

static struct request * get_req(void)
{
	unsigned long flags;
	struct request * req;

	lockb_ide(flags);

try_again:
	if(!list_empty(&req_freelist)){
		req = list_entry(req_freelist.next, list, struct request);
		list_del1(&req->list);
	}else{ /* wake up in ide_bottom */
		sleepon(&req_wait); // cannot unlockb_ide(flags) before sleepon
		goto try_again;
	}
		
	unlockb_ide(flags);

	return req;
}

int ide_open(dev_t dev, int flags)
{
	dev_t minor;

	minor = MINOR(dev);
	if(minor < 0 || minor > 4 || !dev_info[minor].flag)
		return -1;

	return 0;
}

size_t ide_size(dev_t dev)
{
	dev_t minor;

	minor = MINOR(dev);
	if(minor < 0 || minor > 4 || !dev_info[minor].flag){
		DIE("bad device ... %d!\n", minor);
		return 0;
	}

	return dev_info[minor].num_sec * 512;
}

int ide_rw_block(int rw_mode, struct buf_head * buf)
{
	dev_t dev;
	unsigned long flags;
	struct request * req;

	dev = MINOR(buf->b_dev);
	if(dev < 0 || dev > 4 || !dev_info[dev].flag){
		buf->b_flags |= B_ERROR;
		DIE("bad device ... %d!\n", dev);
		return 0;
	}

	if((dev_info[dev].num_sec / 2 - 1) < buf->b_blocknr){
		buf->b_flags |= B_ERROR;
		DIE("bad block %d...!\n", buf->b_blocknr);
		return 0;
	}

	req = get_req();
	req->sec = dev_info[dev].start_sec + buf->b_blocknr * 2;
	req->nsec = 2; /* always 2 */
	req->rw_mode = rw_mode;
	req->buf = buf;

	lockb_ide(flags);

	if(cur_req){
		if(list_empty(&cur_req->list))
			list_add_head(&cur_req->list, &req->list);
		else{
			list_t * tmp, * pos;
			struct request * prev, * next;

			prev = cur_req;

			foreach(tmp, pos, &cur_req->list)
				prev = list_entry(tmp, list, struct request);
				next = list_entry(tmp->next,
					list, struct request);

				if((prev->sec < req->sec)
					&& (next->sec >= req->sec))
					break;
			endeach(&cur_req->list);

			/* req after prev */
			list_add_head(&prev->list, &req->list);
		}

		unlockb_ide(flags);

		return 1;
	}

	cur_req = req;
	list_head_init(&cur_req->list); /* XXX */

	unlockb_ide(flags);

	return finish_cmd();	
}

void get_ide_bios_info(void)
{
        unsigned short * ver_add;
	unsigned char * disk_add;
	unsigned int ver_cs, ver_offset, ver_line;

	/* bios C/H/S */
	ver_add = (unsigned short *)0x104;
	ver_offset = ver_add[0];
	ver_cs = ver_add[1];
	ver_line= ver_cs * 0x10 + ver_offset;
	disk_add = (unsigned char *)ver_line; 
	
	cyl =  *((unsigned short *)&disk_add[0]);
	head = disk_add[2];
	sec = disk_add[14];
	precomp = (*(unsigned short *)((&disk_add[5]))) >> 2;
}

static void get_ide_info(void)
{
	int i;
	unsigned char id[512]; 
	unsigned int lba_capacity;
	struct partition * par_ptr;

#define CAPACITY_OFF	120

        outb(0xa0, IDE_CURRENT);
        outb(0xec, IDE_STATUS);
        ide_status_ok();
	ide_io_read(IDE_DATA, id, 256);
	
	lba_capacity = *((unsigned int *)&id[CAPACITY_OFF]);

	/* lba mode, compute again */
	cyl = lba_capacity / (head * sec);
	printk("Your disk C:%d, H:%d, S:%d, PRECOMP:%d, LBA:%d\n", 
			cyl, head, sec, precomp, lba_capacity);

	ide_cmd(0, READ_CMD, 0, 1);

	ide_status_ok();
	ide_io_read(IDE_DATA, id, 256);

#if 1
	printk("Partition info:\n");
#endif

	dev_info[0].flag = 1;
	dev_info[0].start_sec = 0;
	dev_info[0].num_sec = lba_capacity;

	par_ptr = (struct partition *)&id[0x1be];
	for(i = 1; i < 5; i++, par_ptr++){
		if(par_ptr->sys_ind == 0){
			dev_info[i].flag = 0;
			continue;
		}

		dev_info[i].flag = 1;
		dev_info[i].start_sec = par_ptr->start_sec;
		dev_info[i].num_sec = par_ptr->num_sec;
#if 1 
		printk("%d: %x %d %dM\n", i, par_ptr->sys_ind,
			par_ptr->start_sec, par_ptr->num_sec / 2048);
#endif
	}
}

extern int pci_ide_detect(void);

void ide_init(void)
{
	int i;
	extern unsigned char cfgspace[256];

	ide_cmd_func = ide_cmd;	

	if(pci_ide_detect()){
		if(cfgspace[0x09] & 0x80){ // programming interface
			dma_base_addr
				= *(unsigned long *)&cfgspace[0x20] & (~1UL);
			printk("DMA support, base addr: 0x%08x\n",
				dma_base_addr);
			ide_cmd_func = dma_ide_cmd;
			dma_ide_flag = 1;

			if(((unsigned long)&ide_prd & 0x03)
				|| ((unsigned long)ide_prdbuff & 0x01))
				DIE("how to tell gcc align?\n");
		}
	}

	list_head_init(&req_freelist);

	for(i = 0; i < REQQ_SIZE - 1; i++)
		list_add_head(&req_freelist, &reqq[i].list);

	set_bottom(IDE_B, ide_bottom);
	put_irq_handler(0x0e, ide_intr);

	/* outb only for not causing intr in vmware */
	//outb(inb(0x21) | 0x04, 0x21);
	outb(inb(0xa1) | 0x40, 0xa1);
	get_ide_info();
	//outb(inb(0x21) & 0xfb, 0x21);
	outb(inb(0xa1) & 0xbf, 0xa1);
}
