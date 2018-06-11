#include <asm/io.h>
#include <asm/system.h>
#include <cnix/string.h>
#include <cnix/wait.h>
#include <cnix/kernel.h>
#include <cnix/driver.h>

extern unsigned char leds;
extern void setleds(void);

#define color_ch(vga, ch) \
do{ \
	ch = ch | vga->v_attr; \
	if(vga->v_color_reverse) \
		ch ^= 0xff00; \
}while(0)

static void back_copy(unsigned short * dest, unsigned short * src, int count)
{
	unsigned short * d = dest + count, * s = src + count;

	for(; count >= 2; count -= 2){
		d -= 2;
		s -= 2;
		*((unsigned long *)d) = *((unsigned long *)s);
	}

	if(count >= 1){
		d -= 1;
		s-= 1;
		*d = *s;
	}
}

static void front_copy(unsigned short * dest, unsigned short * src, int count)
{
	for(; count >= 2; count -= 2){
		*((unsigned long *)dest) = *((unsigned long *)src);
		dest += 2;
		src += 2;
	}

	if(count >= 1)
		*dest = *src;
}

static void vidmem_set(unsigned short * dest, unsigned short ch, int count)
{
	for(; count > 0; count--)
		*dest++ = ch;
}

#define VGA_CRT_INX	0x3d4
#define VGA_CRT_DATA 	0x3d5

#define REG_ORG		12
#define REG_CUR		14	

#define DEF_COLOR	0x0700

typedef struct vga_struct vga_t;

vga_t vgas[VGA_NUM];
vga_t * cur_vga = &vgas[0];

/*
 * caller should have locked interrupt
 */
static void set_vga(int reg, unsigned short val)
{
	outb(reg, VGA_CRT_INX);
	outb(0xff & (val >> 8), VGA_CRT_DATA);
	outb(reg + 1, VGA_CRT_INX);
	outb(0xff & val, VGA_CRT_DATA);
}

static void vga_flush(vga_t * vga)
{
	unsigned long tmp, flags;
	unsigned short * vidmem;

	vidmem = (unsigned short *)vga->v_realvidmem + vga->v_curpos;

	if(vga->v_idx){
		front_copy(vidmem, vga->v_data, vga->v_idx);

		vga->v_idx = 0;
	}
	
	tmp  = vga->v_org + cols * vga->v_currow + vga->v_curcol;
	if(EQ(tmp, vga->v_curpos))
		return;

	vga->v_curpos = tmp;

	lock(flags);	
	if(EQ(vga, cur_vga))
		set_vga(REG_CUR, vga->v_curpos);
	unlock(flags);
}

extern struct tty_struct * cur_tty;

void select_vga(int index)
{
	int curidx;
	unsigned long flags;
	struct vga_struct * vga;

	curidx = cur_vga - vgas;

	if(EQ(index, -1))
		curidx = EQ(curidx, 0) ? VGA_NUM - 1 : curidx - 1;
	else if(EQ(index, -2))
		curidx = EQ(curidx, VGA_NUM - 1) ? 0 : curidx + 1;
	else if((index < -2) || (index > VGA_NUM - 1))
		return;
	else
		curidx = index;

	vga = &vgas[curidx];

	/* no need to switch, return */
	if(EQ(cur_vga, vga))
		return;	

	cur_vga->v_leds = leds;

	memcpy(cur_vga->v_vidmem, (void *)VID_MEM, VID_SIZE);
	memcpy((void *)VID_MEM, vga->v_vidmem, VID_SIZE);

	cur_vga->v_realvidmem = (unsigned long)cur_vga->v_vidmem;
	vga->v_realvidmem = VID_MEM;

	cur_vga = &vgas[curidx];
	cur_tty = cur_vga->v_tty;

	lock(flags);

	set_vga(REG_ORG, cur_vga->v_org);
	set_vga(REG_CUR, cur_vga->v_curpos);

	leds = cur_vga->v_leds;
	setleds();
	
	unlock(flags);
}

static int hpstack[16]; /* 16 is enough */
static int hpsidx = 0;

#define min(x, y)	((x) >= (y) ? (y) : (x))

/*
 *  history screen page up
 */
void vga_begin_page_up(void)
{
	int org, hp;
	int cur_pos;
	unsigned long flags;

	hp = cur_vga->v_history_pos;

	if(EQ(hp, -1)){
		hpsidx = 0;
		hp = org = cur_vga->v_org;	
	}else{
		if((hp > cur_vga->v_org)
			&& ((hp -  2 * cols * lines) < cur_vga->v_org))
			return;

		org = hp;
	}

	hpstack[hpsidx++] = org;

	org -= cols * lines;
	if (org < cur_vga->v_start)
		org = cur_vga->v_end - cols * (lines - 1)
			- (cols * lines - (hp - cur_vga->v_start));

	cur_pos = org + cols * lines;

	lock(flags);
	set_vga(REG_ORG, org);
	set_vga(REG_CUR, cur_pos);
	unlock(flags);

	// save current history postion
	cur_vga->v_history_pos = org;	
}

/*
 * history screen end page up
 */
void vga_end_page_up(void)
{
	unsigned long flags;

	lock(flags);
	set_vga(REG_ORG, cur_vga->v_org);
	set_vga(REG_CUR, cur_vga->v_curpos);
	unlock(flags);

	cur_vga->v_history_pos = -1;
	hpsidx = 0;
}

/*
 * history screen page down
 */
void vga_begin_page_down(void)
{
	unsigned long flags;
	int cur_pos, org = cur_vga->v_history_pos;
	
	// The SHIFT+PAGEUP isn't invoked.
	if (EQ(org, -1) || (hpsidx < 1)){
		return;
	}

	org = hpstack[--hpsidx];
	cur_pos = org + cols * lines;

	lock(flags);	
	set_vga(REG_ORG, org);
	set_vga(REG_CUR, cur_pos);
	lock(flags);	

	// save current history postion
	cur_vga->v_history_pos = org;
}

static void scroll_up(vga_t * vga)
{
	unsigned long flags;
	int start, end, howmuch;
	unsigned short * src, * dest, ch, * vidmem;

	start = vga->v_scroll_start;
	end = vga->v_scroll_end;
	howmuch = end - start;

	if(howmuch < (lines - 1)){
		dest = (unsigned short *)vga->v_realvidmem
			+ vga->v_org + start * cols;
		src = dest + cols;
		front_copy(dest, src, howmuch * cols);

		ch = ' ';
		color_ch(vga, ch);

		vidmem = dest + howmuch * cols;
		vidmem_set(vidmem, ch, cols);

		return;
	}

	if((vga->v_org + cols * (lines + 1)) <= vga->v_end)
		vga->v_org += cols;
	else{
		vga_flush(vga); /* take me how a long time :-) */

		dest = (unsigned short *)vga->v_realvidmem + vga->v_start;
		src = (unsigned short *)vga->v_realvidmem + (vga->v_org + cols);
		front_copy(dest, src, (lines - 1) * cols);

		vga->v_org = vga->v_start;
	}

	lock(flags);
	if(EQ(vga, cur_vga))
		 set_vga(REG_ORG, vga->v_org);
	unlock(flags);
	
	ch = ' ';
	color_ch(vga, ch);

	vidmem = (unsigned short *)vga->v_realvidmem
		+ (vga->v_org + (lines - 1) * cols);
	vidmem_set(vidmem, ch, cols);
}

static void scroll_down(vga_t * vga)
{
	unsigned long flags;
	int start, end, howmuch;
	unsigned short * src, * dest, ch, * vidmem;

	start = vga->v_scroll_start;
	end = vga->v_scroll_end;
	howmuch = end - start;

	if(howmuch < (lines - 1)){
		src = (unsigned short *)vga->v_realvidmem
			+ vga->v_org + start * cols;
		dest = src + cols;
		back_copy(dest, src, howmuch * cols);

		ch = ' ';
		color_ch(vga, ch);

		vidmem_set(src, ch, cols);

		return;
	}

	if((vga->v_org - cols) >= vga->v_start){
		vga->v_org -= cols;
	}else{
		vga_flush(vga); /* XXX */

		dest = (unsigned short *)vga->v_realvidmem
			+ (vga->v_end - (lines - 1) * cols);
		src = (unsigned short *)vga->v_realvidmem + vga->v_org;
		back_copy(dest, src, (lines - 1) * cols);

		vga->v_org = vga->v_end - lines * cols;
	}

	lock(flags);
	if(EQ(vga, cur_vga))
		set_vga(REG_ORG, vga->v_org);
	unlock(flags);

	ch = ' ';
	color_ch(vga, ch);

	vidmem = (unsigned short *)vga->v_realvidmem + vga->v_org;
	vidmem_set(vidmem, ch, cols);
}

#include <asm/system.h>
#include <cnix/timer.h>

static int beeping = 0;

static void beep_stop(void * data)
{
	unsigned long flags;

	lock(flags);
	outb(inb(0x61) & ~3, 0x61);	
	unlock(flags);

	beeping = 0;
}

synctimer_t beep_timer;

void beep_start(void)
{
	unsigned long flags;

	lock(flags);

	if(beeping){
		unlock(flags);
		return;
	}

	beep_timer.expire = HZ / 10;
	beep_timer.timerproc = beep_stop;
	synctimer_set(&beep_timer);

	/*
	 * (0x630 * HZ) is just larger than (TIMER_FREQ / 10),
	 * so beep_stop will be executed before beeping stops naturally.
	 */
	outb(0xb6, 0x43);
	outb(0x30, 0x42);
	outb(0x06, 0x42);
	outb(inb(0x61) | 3, 0x61);

	unlock(flags);

	beeping = 1;
}

static void beep_proc(vga_t * vga)
{
	beep_start();
}

static void bs_proc(vga_t * vga)
{
	--vga->v_curcol;
	if(vga->v_curcol < 0){
		--vga->v_currow;
		if(vga->v_currow >= 0)
			vga->v_curcol += cols;
		else{
			vga->v_curcol = 0;
			vga->v_currow = 0;
		}
	}
}

static void nl_proc(vga_t * vga)
{
	if((vga->v_tty->t_termios.c_oflag & OPOST)
		&& (vga->v_tty->t_termios.c_oflag & ONLCR))
		vga->v_curcol = 0;

	if(EQ(vga->v_currow, vga->v_scroll_end))
		scroll_up(vga);
	else
		vga->v_currow++;
}

static void cr_proc(vga_t * vga)
{
	vga->v_curcol = 0;
}

static void tab_proc(vga_t * vga)
{
	vga->v_curcol = (vga->v_curcol + 8) & (~(8 - 1));

	if(vga->v_curcol > cols - 1){
		vga->v_curcol -= cols - 1;
		if(EQ(vga->v_currow, vga->v_scroll_end))
			scroll_up(vga);
		else
			vga->v_currow++;
	}
}

static void esc_proc(vga_t * vga)
{
	vga->v_escape = 1;
}

typedef void (*char_proc_t)(vga_t *);

static struct char_table_struct{
	char ch;
	char_proc_t char_proc;
}char_table[] = {
	{ '\n', nl_proc },
	{ 033, esc_proc },
	{ '\t', tab_proc },
	{ '\b', bs_proc },
	{ 007,  beep_proc },
	{ '\r', cr_proc },
};

#define CHAR_TABLE_SIZE (sizeof(char_table) / sizeof(struct char_table_struct))

static void escape0(vga_t * vga, unsigned char ch)
{
	if(ZERO(ch))
		return;

	if(ch < ' '){
		int i;

		for(i = 0; i < CHAR_TABLE_SIZE; i++)
			if(EQ(char_table[i].ch, ch)){
				char_table[i].char_proc(vga);
				vga_flush(vga);
				return;
			}
	}

	if((vga->v_idx > VBUFFERSIZE - 1) || (vga->v_curcol > (cols - 1))){
		if(EQ(vga->v_currow, vga->v_scroll_end))
			scroll_up(vga);
		else
			vga->v_currow++;

		vga->v_curcol = 0;
		vga_flush(vga);
	}

	vga->v_data[vga->v_idx] = ch;
	color_ch(vga, vga->v_data[vga->v_idx]);

	vga->v_idx++;
	vga->v_curcol++;
}

/* 
 * ESC [nA, ESC [nB, ESC [nC, ESC [nD
 * ESC [sJ, ESC [sK
 * ESC [m;nH
 * ESC [nL, ESC [nM, ESC [nP, ESC [n@, ESC [nm
 * ESC M
 * state 1: see ESC, state 2: see [, state 3: see '0'-'9', state 4: see ';'
 * ...
 */

static void escape1(vga_t * vga, unsigned char ch)
{
	if(EQ(ch, '[')){
		int i;
		for(i = 0; i < ESC_ARGV_NUM; i++)
			vga->v_esc_argv[i] = -1; // invalid value
		vga->v_escape = 2;
		return;
	}else if(EQ(ch, '(')){
		vga->v_escape = 9;
		return;
	}else if(EQ(ch, ')')){
		vga->v_escape = 10;
		return;
	}

	switch(ch){
	case 'D': /* Index */
		break;
	case 'E': /* Next Line */
		break;
	case 'M': /* Reverse index */
		if(EQ(vga->v_currow, vga->v_scroll_start))
			scroll_down(vga);
		else
			vga->v_currow--;

		vga_flush(vga);
		break;
	case '7': /* Save Cursor and attributes    */
		vga->v_bak_currow = vga->v_currow;
		vga->v_bak_curcol = vga->v_curcol;
		vga->v_bak_curpos = vga->v_curpos;
		vga->v_bak_attr = vga->v_attr;
		vga_flush(vga);
		break;
	case '8': /* Restore cursor and attributes */
		if(vga->v_bak_curpos >= 0){
			vga->v_currow = vga->v_bak_currow;
			vga->v_curcol = vga->v_bak_curcol;
			vga->v_curpos = vga->v_bak_curpos;
			vga->v_attr = vga->v_bak_attr;
			vga->v_bak_curpos = -1;
			vga->v_bak_attr = DEF_COLOR;
			vga_flush(vga);
		}
		break;
	case '1': /* Graphic proc. option: On  */
	case '2': /* Graphic proc. option: Off */
	case '=': /* Keypad mode: Application */
	case '>': /* Keypad mode: Numeric     */
		break;
	default:
		break;
	}

	vga->v_escape = 0;
}

#define check_col(vga) \
do{ \
	if(vga->v_curcol < 0) \
		vga->v_curcol = 0; \
	if(vga->v_curcol > cols - 1) \
		vga->v_curcol = cols - 1; \
}while(0)

#define check_row(vga) \
do{ \
	if(vga->v_currow < 0) \
		vga->v_currow = 0; \
	if(vga->v_currow > lines - 1) \
		vga->v_currow = lines - 1; \
}while(0)

static void esc_at(vga_t * vga)
{
	int v, count;	
	unsigned short * src, * dest, ch, * vidmem;
	
	v = vga->v_esc_argv[0];

	v = (v <= 0) ? 0 : v - 1;
	if(v > (cols - vga->v_curcol))
		v = cols - vga->v_curcol;

	ch = ' ';
	color_ch(vga, ch);

	count = cols - (vga->v_curcol + v);

	src = (unsigned short *)vga->v_realvidmem + vga->v_curpos;
	dest = src + v;
	back_copy(dest, src, count);
	
	vidmem = src;
	vidmem_set(vidmem, ch, v);
}

static void esc_A(vga_t * vga)
{
	int v = vga->v_esc_argv[0];

	vga->v_currow -= ((v <= 0) ? 1 : v);
	check_row(vga);
}

static void esc_B(vga_t * vga)
{
	int v = vga->v_esc_argv[0];

	vga->v_currow += ((v <= 0) ? 1 : v);
	check_row(vga);
}

static void esc_C(vga_t * vga)
{
	int v = vga->v_esc_argv[0];

	vga->v_curcol += ((v <= 0) ? 1 : v);
	check_col(vga);
}

static void esc_D(vga_t * vga)
{
	int v = vga->v_esc_argv[0];

	vga->v_curcol -= ((v <= 0) ? 1 : v);
	check_col(vga);
}

static void esc_E(vga_t * vga)
{
	int v = vga->v_esc_argv[0];

	vga->v_currow += ((v <= 0) ? 1 : v);
	check_row(vga);
	vga->v_curcol = 0;
}

static void esc_F(vga_t * vga)
{
	int v = vga->v_esc_argv[0];

	vga->v_currow -= ((v <= 0) ? 1 : v);
	check_row(vga);
	vga->v_curcol = 0;
}

static void esc_G(vga_t * vga)
{
	int v = vga->v_esc_argv[0];

	v = (v <= 0) ? 0 : v - 1;

	vga->v_curcol = v;
	check_col(vga);
}

static void esc_H(vga_t * vga)
{
	int v;

	v  = vga->v_esc_argv[1];
	vga->v_curcol = (v <= 0) ? 0 : v - 1;
	check_col(vga);

	v  = vga->v_esc_argv[0];
	vga->v_currow = (v <= 0) ? 0 : v - 1;
	check_row(vga);
}

static void esc_J(vga_t * vga)
{
	int v, start, count;
	unsigned short ch, * vidmem;

	v = vga->v_esc_argv[0];
	if(v < 0)
		v = 0;

	switch(v){
	case 0:
		start = vga->v_curpos;
		count = lines * cols - (vga->v_curpos - vga->v_org);
		break;
	case 1:
		start = vga->v_org;
		count = vga->v_curpos - vga->v_org;
		break;
	case 2:
		start = vga->v_org;
		count = lines * cols;
		break;
	default:
		return;
	}

	ch = ' ';
	color_ch(vga, ch);

	vidmem = (unsigned short *)vga->v_realvidmem + start;
	vidmem_set(vidmem, ch, count);
}

static void esc_K(vga_t * vga)
{
	int v, start, count;
	unsigned short ch, * vidmem;

	v = vga->v_esc_argv[0];
	if(v < 0)
		v = 0;

	switch(v){
	case 0:
		start = vga->v_curpos;
		count = cols - vga->v_curcol;
		break;
	case 1:
		start = vga->v_curpos - vga->v_curcol;
		count = vga->v_curcol;
		break;
	case 2:
		start = vga->v_curpos - vga->v_curcol;
		count = cols;
		break;
	default:
		return;
	}

	ch = ' ';
	color_ch(vga, ch);

	vidmem = (unsigned short *)vga->v_realvidmem + start;
	vidmem_set(vidmem, ch, count);
}

static void esc_L(vga_t * vga)
{
	int v, count;
	unsigned short * src, * dest, ch, * vidmem;

	v = vga->v_esc_argv[0];

	v = (v <= 0) ? 0 : v - 1;
	if(v > lines - vga->v_currow)
		v = lines - vga->v_currow;

	ch = ' ';
	color_ch(vga, ch);

	count = lines - (v + vga->v_currow);

	src = (unsigned short *)vga->v_realvidmem
		+ (vga->v_curpos - vga->v_curcol);
	dest = src + v * cols;
	back_copy(dest, src, count * cols);
	
	vidmem = src;
	vidmem_set(vidmem, ch, (v * cols));
}

static void esc_M(vga_t * vga)
{
	int v, count;
	unsigned short * src, * dest, ch, * vidmem;

	v = vga->v_esc_argv[0];

	v = (v <= 0) ? 0 : v - 1;
	if(v > lines - vga->v_currow)
		v = lines - vga->v_currow;

	ch = ' ';
	color_ch(vga, ch);

	count = lines - (v + vga->v_currow);

	dest = (unsigned short *)vga->v_realvidmem
		+ (vga->v_curpos - vga->v_curcol);
	src = dest + v * cols;
	
	front_copy(dest, src, count * cols);
	vidmem = (unsigned short *)dest + (count * cols);
	vidmem_set(vidmem, ch, (v * cols));
}

static void esc_P(vga_t * vga)
{
	int v, count;	
	unsigned short * src, * dest, ch, * vidmem;
	
	v = vga->v_esc_argv[0];

	if(v <= 0)
		v = 1;
	else if(v > (cols - vga->v_curcol))
		v = cols - vga->v_curcol;

	ch = ' ';
	color_ch(vga, ch);

	count = cols - (vga->v_curcol + v);

	dest = (unsigned short *)vga->v_realvidmem + vga->v_curpos;
	src = dest + v;	
	front_copy(dest, src, count);

	vidmem = (unsigned short *)dest + (count << 1);
	vidmem_set(vidmem, ch, v);
}

static void esc_X(vga_t * vga)
{
	int v;
	unsigned short * dest, ch;
	
	v = vga->v_esc_argv[0];

	if(v <= 0)
		v = 1;
	else if(v > (cols - vga->v_curcol))
		v = cols - vga->v_curcol;

	ch = ' ';
	color_ch(vga, ch);

	dest = (unsigned short *)vga->v_realvidmem + vga->v_curpos;
	vidmem_set(dest, ch, v);
}

static void esc_a(vga_t * vga)
{
	int v = vga->v_esc_argv[0];

	if(v <= 0)
		v = 1;

	vga->v_curcol = v;
	check_col(vga);
}

static void esc_c(vga_t * vga)
{
	// XXX: answer-ESC[?6c:-I'm a VT102
}

static void esc_d(vga_t * vga)
{
	int v = vga->v_esc_argv[0];

	vga->v_currow = (v <= 0) ? 0 : v - 1;
	check_row(vga);
}

static void esc_e(vga_t * vga)
{
#if 0
	int v = vga->v_esc_argv[0];

	if(v <= 0)
		v = 1;

	vga->v_currow = v;
	check_row(vga);
#endif
}

static void esc_g(vga_t * vga)
{
	/* XXX: clear the tab stop at the current position */
}

/*
 *					To Set		To Reset
 * Mode Name		Mode				Mode
 * Line feed/new line	New Line	ESC [20h	Line Feed	ESC [20l
 * Cursor key mode	Application	ESC [?1h	Cursor		ESC [?l
 * ANSI/VT52 mode	ANSI		N/A		VT52		ESC [?2l
 * Column mode		132 Col		ESC [?3h	80 Col		ESC [?3l
 * Scrolling mode	Smooth		ESC [?4h	Jump		ESC [?4l
 * Screen mode		Reverse		ESC [?5h	Normal		ESC [?5l
 * Origin mode		Relative	ESC [?6h	Absolute	ESC [?6l
 * Wraparound		On		ESC [?7h	Off		ESC [?7l
 * Auto repeat		On		ESC [?8h	Off		ESC [?8l
 * Interlace		On		ESC [?9h	Off		ESC [?9l
 * Graphic proc. option	On		ESC 1		Off		ESC 2
 * Keypad mode		Application	ESC =		Numeric		ESC >
 */

static void esc_h(vga_t * vga)
{

}

static void esc_l(vga_t * vga)
{

}

static unsigned char colors[] = { 0, 4, 2, 6, 1, 5, 3, 7 };

static void esc_m0(vga_t * vga)
{
	int v, i = 0;
	unsigned short attr = vga->v_attr, mask = 0;

	v = vga->v_esc_argv[0];
	switch(v){
	case 0: /* normal */
		vga->v_attr = DEF_COLOR;
		vga->v_color_reverse = 0;
		return;
	case 1: /* bright, usually turns on bold */
		return;
	case 2: /* dim */
		return;
	case 3: /* underline */
		return;
	case 5: /* blink */
		return;
	case 7: /* reserve */
		vga->v_color_reverse = 1;
		return;
	case 8: /* hidden */
		return;
	default:
		break;
	}

	/*
	 * fg: 30    31  32    33     34   35      36   37
	 * bk: 40    41  42    43     44   45      46   47
	 *     black red green yellow blue magenta cyan white
	 */
	if((v >= 30) && (v <= 37)){
		v -= 30;
		i = 8;
		mask = 0xf0ff;
	}else if((v >= 40) && (v <= 47)){
		v -= 40;
		i = 12;
		mask = 0x0fff;
	}	

	if(i)
		vga->v_attr = (attr & mask) | (colors[v] << i);
}

static void esc_m(vga_t * vga)
{
	int v;	

	v = vga->v_esc_argv[0];	
	if(v < 0) /* default */
		v = 0;

	vga->v_esc_argv[0] = v;
	esc_m0(vga);

	v = vga->v_esc_argv[1];
	if(v >= 0){
		vga->v_esc_argv[0] = v;
		esc_m0(vga);
	}

	v = vga->v_esc_argv[2];
	if(v >= 0){
		vga->v_esc_argv[0] = v;
		esc_m0(vga);
	}

	v = vga->v_esc_argv[3];
	if(v >= 0){
		vga->v_esc_argv[0] = v;
		esc_m0(vga);
	}
}

static void esc_n(vga_t * vga)
{

}

static void esc_q0(vga_t * vga)
{
	int v = vga->v_esc_argv[0];

	if(EQ(v, 0)){
		leds = 0;
		setleds();
	}else if((v >= 1) && (v <= 3)){
		leds |= 1 << (v - 1);
		setleds();
	}
}

static void esc_q(vga_t * vga)
{
	int v;

	v = vga->v_esc_argv[0];
	if(v < 0)
		v = 0;

	vga->v_esc_argv[0] = v;
	esc_q0(vga);

	v = vga->v_esc_argv[1];
	if(v >= 0){
		vga->v_esc_argv[0] = v;
		esc_q0(vga);
	}

	v = vga->v_esc_argv[2];
	if(v >= 0){
		vga->v_esc_argv[0] = v;
		esc_q0(vga);
	}

	v = vga->v_esc_argv[3];
	if(v >= 0){
		vga->v_esc_argv[0] = v;
		esc_q0(vga);
	}
}

/*
 * Scrolling Region
 */
static void esc_r(vga_t * vga)
{	
	int start = vga->v_esc_argv[0];
	int end = vga->v_esc_argv[1];

	start = (start <= 0) ? 0 : start - 1;
	end = (end <= 0) ? 0 : end - 1;

	if(start >= end) /* do nothing */
		return;

	vga->v_scroll_start = start;
	vga->v_scroll_end = end;
}

static void esc_s(vga_t * vga)
{

}

static void esc_u(vga_t * vga)
{

}

typedef void (*esc_char_proc_t)(vga_t *);

static struct esc_char_table_struct{
	char ch;
	esc_char_proc_t esc_char_proc;
}esc_char_table[] = {
	{ '@', esc_at },
	{ 'A', esc_A },
	{ 'B', esc_B },
	{ 'C', esc_C },
	{ 'D', esc_D },
	{ 'E', esc_E },
	{ 'F', esc_F },
	{ 'G', esc_G },
	{ 'H', esc_H },
	{ 'J', esc_J },
	{ 'K', esc_K },
	{ 'L', esc_L },
	{ 'M', esc_M },
	{ 'P', esc_P },
	{ 'X', esc_X },
	{ 'a', esc_a },
	{ 'c', esc_c },
	{ 'd', esc_d },
	{ 'e', esc_e },
	{ 'f', esc_H }, /* the same with 'H' */
	{ 'g', esc_g },
	{ 'h', esc_h },
	{ 'l', esc_l },
	{ 'm', esc_m },
	{ 'n', esc_n },
	{ 'q', esc_q },
	{ 'r', esc_r },
	{ 's', esc_s },
	{ 'u', esc_u },
};

#define ESC_CHAR_TABLE_SIZE \
	(sizeof(esc_char_table) / sizeof(struct esc_char_table_struct))

esc_char_proc_t esc_char_proc_table[256];

void select_vga_init(int index)
{
	int i;
	unsigned long flags;
	struct vga_struct * vga;

	for(i = 0; i < 256; i++)
		esc_char_proc_table[i] = NULL;

	for(i = 0; i < ESC_CHAR_TABLE_SIZE; i++){
		struct esc_char_table_struct * e = &esc_char_table[i];
		esc_char_proc_table[(int)e->ch] = e->esc_char_proc;
	}

	vga = &vgas[index];

	cur_vga = vga;
	cur_tty = cur_vga->v_tty;

	cur_vga->v_realvidmem = VID_MEM;

	lock(flags);

	set_vga(REG_ORG, cur_vga->v_org);
	set_vga(REG_CUR, cur_vga->v_curpos);
	
	leds = cur_vga->v_leds;
	setleds();

	unlock(flags);	
}

static void escape2(vga_t * vga, unsigned char ch)
{
	if((ch >= '0') && (ch <= '9')){
		vga->v_esc_argv[0] = ch - '0';
		vga->v_escape = 3;
		return;
	}

	if(EQ(ch, '?')){
		vga->v_escape = 8;
		return;
	}
	
	if(esc_char_proc_table[ch]){
		esc_char_proc_table[ch](vga);
		vga_flush(vga);
	}

	vga->v_escape = 0;
}

static void escape3(vga_t * vga, unsigned char ch)
{
	if((ch >= '0') && (ch <= '9')){
		vga->v_esc_argv[0] = vga->v_esc_argv[0] * 10 + ch - '0';
		return;
	}

	if(EQ(ch, ';')){
		vga->v_escape = 4;
		return;
	}

	if(esc_char_proc_table[ch]){
		esc_char_proc_table[ch](vga);
		vga_flush(vga);
	}

	vga->v_escape = 0;
}

static void escape4(vga_t * vga, unsigned char ch)
{
	if((ch >= '0') && (ch <= '9')){
		vga->v_esc_argv[1] = ch - '0';
		vga->v_escape = 5;
		return;
	}

	if(esc_char_proc_table[ch]){
		esc_char_proc_table[ch](vga);
		vga_flush(vga);
	}

	vga->v_escape = 0;
}

static void escape5(vga_t * vga, unsigned char ch)
{
	if((ch >= '0') && (ch <= '9')){
		vga->v_esc_argv[1] = vga->v_esc_argv[1] * 10 + ch - '0';
		return;
	}

	if(EQ(ch, ';')){
		vga->v_escape = 6;
		return;
	}

	if(esc_char_proc_table[ch]){
		esc_char_proc_table[ch](vga);
		vga_flush(vga);
	}

	vga->v_escape = 0;
}

static void escape6(vga_t * vga, unsigned char ch)
{
	if((ch >= '0') && (ch <= '9')){
		vga->v_esc_argv[2] = ch - '0';
		vga->v_escape = 7;
		return;
	}

	if(esc_char_proc_table[ch]){
		esc_char_proc_table[ch](vga);
		vga_flush(vga);
	}

	vga->v_escape = 0;
}

static void escape7(vga_t * vga, unsigned char ch)
{
	if((ch >= '0') && (ch <= '9')){
		vga->v_esc_argv[2] = vga->v_esc_argv[2] * 10 + ch - '0';
		return;
	}

	if(esc_char_proc_table[ch]){
		esc_char_proc_table[ch](vga);
		vga_flush(vga);
	}

	if(EQ(ch, ';')){
		vga->v_escape = 11;
		return;
	}

	vga->v_escape = 0;
}

static void escape8(vga_t * vga, unsigned char ch)
{
	if((ch >= '0') && (ch <= '9')){
		vga->v_esc_argv[0] = vga->v_esc_argv[0] * 10 + ch - '0';
		return;
	}

	if(esc_char_proc_table[ch]){
		esc_char_proc_table[ch](vga);
		vga_flush(vga);
	}

	vga->v_escape = 0;
}

/*
 * Character Set		G0 Designator	G1 Designator
 * United Kingdon (UK)		ESC ( A		ESC ) A		
 * United States (USASCII)	ESC ( B		ESC ) B
 * Special graphics characters	ESC ( 0		ESC ) 0
 *   and line drawing set
 * Alternate character ROM	ESC ( 1		ESC ) 1
 * Alternate character ROM	ESC ( 2		ESC ) 2
 *   special graphics characters
 */
static void escape9(vga_t * vga, unsigned char ch)
{
	switch(ch){
	case 'A':
	case 'B':
	case '0':
	case '1':
	case '2':
		break;
	default:
		break;
	}

	vga->v_escape = 0;
}

static void escape10(vga_t * vga, unsigned char ch)
{
	switch(ch){
	case 'A':
	case 'B':
	case '0':
	case '1':
	case '2':
		break;
	default:
		break;
	}

	vga->v_escape = 0;
}

static void escape11(vga_t * vga, unsigned char ch)
{
	if((ch >= '0') && (ch <= '9')){
		vga->v_esc_argv[3] = ch - '0';
		vga->v_escape = 12;
		return;
	}

	if(esc_char_proc_table[ch]){
		esc_char_proc_table[ch](vga);
		vga_flush(vga);
	}

	vga->v_escape = 0;
}

static void escape12(vga_t * vga, unsigned char ch)
{
	if((ch >= '0') && (ch <= '9')){
		vga->v_esc_argv[3] = vga->v_esc_argv[3] * 10 + ch - '0';
		return;
	}

	if(esc_char_proc_table[ch]){
		esc_char_proc_table[ch](vga);
		vga_flush(vga);
	}

	vga->v_escape = 0;
}

typedef void (*esc_proc_t)(vga_t *, unsigned char);

static esc_proc_t esc_proc_table[] = {
	escape0,
	escape1,
	escape2,
	escape3,
	escape4,
	escape5,
	escape6,
	escape7,
	escape8,
	escape9,
	escape10,
	escape11,
	escape12,
};

void kputs(const char * s)
{
	unsigned char ch;
	vga_t * vga = &vgas[0]; //cur_vga;

	while((ch = *s) != '\0'){
		esc_proc_table[(int)vga->v_escape](vga, ch);
		s++;
	}

	vga_flush(vga);
}

/*
 * caller should have locked tty
 */
int vga_write(struct tty_struct * tty)
{
	const char * data;
	int i, count;
	vga_t * vga = tty->t_vga;

	data = tty->t_userdata;
	count = tty->t_userdatacount;
	for(i = 0; i < count; i++){
#if 0
		/* catch any escape char for some special program */
		if(strstr(current->myname, "telnet")){
			if(data[i] == '\n')
				kputs("\\n");
			else if(data[i] == '\033')
				kputs("\\033");
			else if(data[i] < ' ')
				printk("0x%x", data[i]);
			else
				printk("%c", data[i]);
		}
#endif
		esc_proc_table[(int)vga->v_escape](vga, data[i]);
	}

	vga_flush(vga);

	return count;
}

void echochar(struct tty_struct * tty, unsigned char ch)
{
	unsigned long flags;

	lockb_kbd(flags);

	if(EQ(tty->t_vga, cur_vga)){
		unlockb_kbd(flags);
		esc_proc_table[(int)tty->t_vga->v_escape](tty->t_vga, ch);

		return;
	}

	unlockb_kbd(flags);

	esc_proc_table[(int)tty->t_vga->v_escape](tty->t_vga, ch);
}

void vga_flush1(struct tty_struct * tty)
{
	unsigned long flags;

	lockb_kbd(flags);
	if(EQ(tty->t_vga, cur_vga)){
		unlockb_kbd(flags);
		vga_flush(tty->t_vga);

		return;
	}

	unlockb_kbd(flags);
}

int vga_getcol(struct tty_struct * tty)
{
	return tty->t_vga->v_curcol;
}

void vga_init(struct tty_struct * tty, int idx)
{
	vga_t * vga;
	int i, scr_size;

	if(idx > (VGA_NUM - 1))
		return;

	scr_size = (VID_SIZE >> 1) / VGA_NUM;

	vga = &vgas[idx];

	tty->t_vga = vga;
	vga->v_tty = tty;
	
	vga->v_currow = 0;
	vga->v_curcol = 0;

	vga->v_start = 0;
	vga->v_end = vga->v_start + ((VID_SIZE >> 1) / cols) * cols;

	/* org is not set to v_start, just for us could see boot messages */
	vga->v_org = vga->v_end - cols * lines;
	vga->v_curpos = vga->v_org + vga->v_currow * cols + vga->v_curcol;

	vga->v_escape = 0;
	vga->v_attr = DEF_COLOR;
	vga->v_idx = 0;

	vga->v_history_pos = -1;

	/* clear screen */
	for(i = 0; i < (VID_SIZE >> 1); i++)
		vga->v_vidmem[i] = ' ' | DEF_COLOR;

	vga->v_realvidmem = (unsigned long)vga->v_vidmem;

	vga->v_bak_curpos = -1;
	vga->v_bak_attr = DEF_COLOR;

	vga->v_scroll_start = 0;
	vga->v_scroll_end = lines - 1;

	vga->v_leds = 0;
	vga->v_color_reverse = 0;
}
