#ifndef _DRIVER_H
#define _DRIVER_H

#include <cnix/tty.h>
#include <cnix/vga.h>
#include <cnix/ide.h>
#include <cnix/timer.h>
#include <cnix/buffer.h>

extern struct vga_struct vgas[];

extern void vga_init(struct tty_struct *, int);
extern void select_vga(int);

extern void do_tty_interrupt(void);

extern void ide_init(void);
extern int ide_rw_block(int, struct buf_head *);

#endif
