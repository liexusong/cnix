#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/errno.h>
#include <cnix/fs.h>
#include <cnix/mm.h>
#include <cnix/kernel.h>
#include <cnix/unistd.h>
#include <cnix/devices.h>

BOOLEAN mmap_check_addr(
	struct mm_struct * mm, unsigned long addr, unsigned long memsz
	)
{
	int i;
	unsigned long start, end;

	if(!memsz)
		DIE("BUG: cannot happen\n");

	for(i = 0; i < mm->mmap_num; i++){
		start = mm->mmap[i].start & PAGE_MASK;
		end = PAGE_ALIGN(mm->mmap[i].end);

		if(addr < start){
			if(addr + memsz > start)
				return FALSE;
		}else if(addr > start){
			if(addr < end)
				return FALSE;
		}else
			return FALSE;
	}

	return TRUE;
}

unsigned long __mmap_find_space(
	struct mm_struct * mm, unsigned long start, size_t length
	)
{
	int i;
	unsigned long start1, end1;

	if(!length)
		DIE("BUG: cannot happen\n");

	if(start & ~PAGE_MASK)
		DIE("BUG: cannot happen\n");

	if(start + length < start)
		DIE("BUG: cannot happen\n");

	// look for after data segment
	if(start < mm->mmap[1].end)
		DIE("BUG: cannot happen\n");

	for(i = 2; i < mm->mmap_num; i++){
		start1 = mm->mmap[i].start & PAGE_MASK;
		end1 = PAGE_ALIGN(mm->mmap[i].end);

		if(start < start1){
			if((start + length) <= start1) 
				break;

			start = end1;
		}else if(start < end1)
			start = end1;
	}

	// reached stack
	if(i >= mm->mmap_num)
		return 0;

	return start;
}

unsigned long mmap_low_addr(struct mm_struct * mm)
{
	// the start of segment right after data
	return mm->mmap[2].start;
}

struct mmap_struct * mmap_find_shared(
	unsigned long start,
	size_t length,
	struct inode * ino,
	off_t offset,
	int prot,
	unsigned long * pg_dir
	)
{
	int i, j;
	struct task_struct * t;
	struct mmap_struct * m;

	for(i = 0; i < NR_TASKS; i++){
		t = task[i];
		if(!t || (t->state == TASK_ZOMBIE))
			continue;
		
		for(j = 0; j < t->mm.mmap_num; j++){
			m = &t->mm.mmap[j];
			if(!m->start)
				break;

			if(!(m->flags & MAP_SHARED))
				continue;

			if(m->ino != ino)
				continue;

			if(m->prot != prot)
				continue;

			if(m->offset != offset)
				continue;

			if(m->length != length)
				continue;

			*pg_dir = t->pg_dir;
			return m;
		}
	}

	return NULL;
}

static BOOLEAN mmap_avail(struct mm_struct * mm)
{
	if(mm->mmap_num < MAX_MMAP_NUM)
		return TRUE;

	return FALSE;
}

static int mmap_anonymous(
	unsigned long start, size_t memsz, int prot, unsigned long pg_dir
	)
{
	int ret, ac, pages;
	unsigned long p, * pg;
	unsigned long addr = start & PAGE_MASK;
	struct mmap_struct mmap;

	ac = 5;
	if(prot & PROT_WRITE)
		ac = 7;

	pages = (PAGE_ALIGN(start + memsz) - addr) >> 12;

	do{
		ret = fill_pgitem(&pg, (unsigned long*)pg_dir, addr);
		if(ret < 0)
			goto errout;

		p = get_one_page();
		if(!p){
			ret = -EAGAIN;
			goto errout;
		}

		pg[to_pt_offset(addr)] = p + 7;

		addr += PAGE_SIZE;
		pages--;
	}while(pages > 0);

	return 0;
errout:
	// free all allocated memory, and unmmap
	mmap.start = start;
	mmap.end = start + memsz;
	free_mmap_pts(&mmap, pg_dir);

	return ret;
}

static int mmap_read_file(
	unsigned long start,
	int fd, off_t offset, size_t filesz, size_t memsz,
	int prot,
	unsigned long pg_dir
	)
{
	int ret;
	int ac, readed, reading;
	unsigned long p, * pg, addr = start;
	struct mmap_struct mmap;

	ret = do_lseek(fd, offset, SEEK_SET);
	if(ret != offset)
		return ret;

	ac = 5;
	if(prot & PROT_WRITE)
		ac = 7;

	for(readed = 0; readed < memsz; readed += reading){
		ret = fill_pgitem(&pg, (unsigned long *)pg_dir, addr);
		if(ret < 0)
			goto errout;

		p = pg[to_pt_offset(addr)];
		if(!p){
			p = get_one_page();
			if(!p){
				ret = -EAGAIN;
				goto errout;
			}

			pg[to_pt_offset(addr)] = p + ac;
		}else // xxx: DIE
			p &= PAGE_MASK;

		reading = PAGE_SIZE - (addr & (PAGE_SIZE - 1));
		if(readed < filesz){
			if(reading > filesz - readed)
				reading = filesz - readed;

			p += addr & (PAGE_SIZE - 1);

			ret = do_read(fd, (char *)p, reading);
			if(ret != reading){
				ret = -EIO;
				goto errout;
			}
		}else{
			if(reading > memsz - readed)
				reading = memsz - readed;
		}
	
		addr += reading;
	}

	return 0;
errout:
	// free all allocated memory, and unmmap
	mmap.start = start;
	mmap.end = addr;
	free_mmap_pts(&mmap, pg_dir);

	return ret;
}

static int mmap_mem_file(
	unsigned long start,
	off_t offset,
	size_t length,
	int prot,
	unsigned long pg_dir
)
{
	int ret;
	int ac, readed, reading;
	unsigned long p, * pg, addr = start;
	struct mmap_struct mmap;

	ac = 5;
	if(prot & PROT_WRITE)
		ac = 7;

	for(readed = 0; readed < length; readed += reading, offset += reading){
		ret = fill_pgitem(&pg, (unsigned long *)pg_dir, addr);
		if(ret < 0)
			goto errout;

		p = pg[to_pt_offset(addr)];
		if(!p){
			pg[to_pt_offset(addr)] = (offset & PAGE_MASK) + ac;
			mem_map[MAP_NR(offset)].count++;
		}

		reading = PAGE_SIZE - (addr & (PAGE_SIZE - 1));
		if(reading > length - readed)
			reading = length - readed;
	
		addr += reading;
	}

	return 0;
errout:
	// free all allocated memory, and unmmap
	mmap.start = start;
	mmap.end = addr;
	free_mmap_pts(&mmap, pg_dir);

	return ret;
}

static void mmap_insert(struct mm_struct * mm, struct mmap_struct * mmap)
{
	int i, j;

	for(i = 0; i < mm->mmap_num; i++){
		if(mm->mmap[i].start > mmap->start)
			break;
	}

	if(!i){
		mm->mmap[0] = *mmap;
		mm->mmap_num += 1;
		return;
	}

	for(j = mm->mmap_num - 1; j >= i; j--)
		mm->mmap[j + 1] = mm->mmap[j];

	mm->mmap[i] = *mmap;
	mm->mmap_num += 1;
}

// param mmap is a member of array mm->mmap
static void mmap_delete(struct mm_struct * mm, int idx)
{
	int i;

	if(mm->mmap[idx].ino)
		iput(mm->mmap[idx].ino);
	
	for(i = idx; i < mm->mmap_num - 1; i++)
		mm->mmap[i] = mm->mmap[i + 1];

	mm->mmap_num -= 1;
	memset(&mm->mmap[mm->mmap_num], 0, sizeof(struct mmap_struct));	
}

int do_mmap(
	unsigned long start,
	size_t length,
	int prot,
	int flags,
	int fd,
	off_t offset,
	size_t memsz,
	unsigned long pg_dir,
	struct mm_struct * mm
	)
{
	int ret;
	struct filp * fp = NULL;
	struct inode * ino = NULL;
	struct mmap_struct mmap;

	// memsz maybe be larger than length when being called in kernel
	if(memsz < length)
		DIE("BUG: cannot happen\n");

	if(!(flags & (MAP_SHARED | MAP_PRIVATE)))
		return -EINVAL;

	if(!(flags & MAP_ANONYMOUS)){
		int rights = 0;

		if(ckfdlimit(fd))
			return -EINVAL;

		fp = fget(fd);
		if(!fp)
			return -EBADF;

		ino = fp->f_ino;

		if(prot & PROT_READ)
			rights |= I_RB;

		if(prot & PROT_WRITE)
			rights |= I_WB;

		if(prot & PROT_EXEC)
			rights |= I_XB;

		if(!admit(ino, rights))
			return -EACCES;
	}else if(flags & MAP_SHARED) // anonymous
		return -EINVAL;

	// MAP_FIXED, not support
	if(start){
		//if(start & ~PAGE_MASK)
		//	return -EINVAL;

		if(cklimit(start) || ckoverflow(start, memsz))
			return -EINVAL;
	}else{
		// get space
		start = mmap_find_space(&current->mm, memsz);
		if(!start)
			return -EINVAL;
	}

	// mmap avail?
	if(!mmap_avail(mm))
		return -ENOMEM;	

	if(flags & MAP_SHARED){
		unsigned long pg_dir1;
		struct mmap_struct * mmap1;

		// to find existed mmap
		mmap1 = mmap_find_shared(
			start, length, ino, offset, prot, &pg_dir1
			);
		if(mmap1){
			BOOLEAN r;

			if(pg_dir1 == pg_dir)
				DIE("BUG: cannot happen\n");

			r = copy_mmap_pts(
				mmap1,
				pg_dir1,
				pg_dir
			);
			if(!r){
				free_mmap_pts(mmap1, pg_dir);
				return -ENOMEM;
			}

			if(!mmap1->ino)
				DIE("BUG: cannot happen\n");

			mmap1->ino->i_count++;

			mmap_insert(mm, mmap1);
			
			return start;
		}
	}

	// setup page table
	if(flags &MAP_ANONYMOUS)
		ret = mmap_anonymous(
			start, memsz, prot, pg_dir
			);
	else if(S_ISREG(ino->i_mode))
		ret = mmap_read_file(
			start, fd, offset, length, memsz, prot, pg_dir
		);
	else if(S_ISCHR(ino->i_mode)){
		int major, minor;
	
		if(offset & ~PAGE_MASK)
			return -EINVAL;

		major = MAJOR(ino->i_zone[0]);
		minor = MINOR(ino->i_zone[0]);

		if((major != 1) && (minor != MEM_DEV)) // dev/mem
			return -EACCES;

		ret = mmap_mem_file(start, offset, length, prot, pg_dir);
	}else
		return -EACCES;

	if(MMAP_FAILED(ret))
		return ret;

	mmap.start = start;
	mmap.end = start + memsz;
	mmap.flags = flags;
	mmap.prot = prot;
	mmap.ino = ino;
	mmap.offset = offset;
	mmap.length = length;

	if(ino)
		ino->i_count++;

	mmap_insert(mm, &mmap);	

	return start;
}

int mmap_writeback(struct mmap_struct * mmap, int length)
{
	int fd, ret;

	if(mmap->flags & MAP_PRIVATE)
		return 0;

	if(!mmap->ino || !S_ISREG(mmap->ino->i_mode))
		return 0;

	if(!(mmap->prot & PROT_WRITE))
		return 0;

	fd = do_inode_open(mmap->ino, O_WRONLY);
	if(fd < 0)
		return fd;

	ret = do_lseek(fd, mmap->offset, SEEK_SET);
	if(ret != mmap->offset){
		do_close(fd);
		return -EIO;
	}

	if(length > mmap->ino->i_size - mmap->offset)
		length = mmap->ino->i_size - mmap->offset;

	// xxx: only write back dirty pages
	ret = do_write(fd, (const char *)mmap->start, length);

	do_close(fd);

	return ret;
}

int do_munmap(unsigned long start, size_t length)
{
	int i, ret;

	for(i = 0; i < current->mm.mmap_num; i++){
		if(current->mm.mmap[i].start == start){
			if(current->mm.mmap[i].length != length)
				return -EINVAL;

			ret = mmap_writeback(&current->mm.mmap[i], length);
			if(ret < 0)
				return ret;

			free_mmap_pts(&current->mm.mmap[i], current->pg_dir);
			mmap_delete(&current->mm, i);

			return 0;
		}
	}

	return -EINVAL;
}

int do_msync(unsigned long start, size_t length, int flags)
{
	// flush mmap
	return -ENOTSUP;
}

void mmap_free_all(struct mm_struct * mm, unsigned long pg_dir)
{
	int i;

	for(i = mm->mmap_num - 1; i >= 0; i -= 1){
		free_mmap_pts(&mm->mmap[i], pg_dir);

		if(mm->mmap[i].ino)
			iput(mm->mmap[i].ino);
	}

	mm->mmap_num = 0;
}

void mmap_flush_all(
	struct mm_struct * mm, unsigned long pg_dir
	)
{
	int i;

	for(i = mm->mmap_num - 1; i >= 0; i -= 1)
		mmap_writeback(&mm->mmap[i], mm->mmap[i].length);
}
