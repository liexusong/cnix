#ifndef _VGA_H
#define _VGA_H

#include <cnix/tty.h>

#define VGA_NUM	2
#define VBUFFERSIZE	256

#define VID_MEM		0xb8000
#define VID_SIZE	0x4000

#define lines		25
#define cols		80

#define ESC_ARGV_NUM	4

struct vga_struct{
	struct tty_struct * v_tty;
	int v_currow, v_curcol;
	int v_start, v_end, v_org, v_curpos;
	int v_escape;
	int v_esc_argv[ESC_ARGV_NUM];
	unsigned short v_attr;
	int v_idx;
	int v_history_pos;
	unsigned short v_data[VBUFFERSIZE];
	unsigned short v_vidmem[VID_SIZE >> 1];
	unsigned long v_realvidmem;
	int v_bak_currow, v_bak_curcol, v_bak_curpos;
	unsigned short v_bak_attr;
	int v_scroll_start, v_scroll_end;
	unsigned char v_leds;
	char v_color_reverse;
};

extern int vga_write(struct tty_struct *);
extern void select_vga_init(int);
extern void select_vga(int);
extern void kputs(const char *);
extern void vga_begin_page_up(void);
extern void vga_end_page_up(void);
extern void vga_begin_page_down(void);

#endif
