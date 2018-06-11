#ifndef _BOTTOM_H
#define _BOTTOM_H

#include <cnix/types.h>

typedef void (*bottomproc_t)(void);

enum bottom_t{
	TIMER_B,
	IDE_B,
	KEYBOARD_B,
	FLOPPY_B,
	PCNET32_B,
	END_B,
};

extern void raise_bottom(int);
extern void lock_bottom(int, unsigned long *);
extern void unlock_bottom(int, unsigned long);
extern void lock_all_bottom(unsigned long *);
extern void unlock_all_bottom(unsigned long);
extern void save_bottom_mask(unsigned long *);
extern void restore_bottom_mask(unsigned long);
extern void set_bottom(int, bottomproc_t);
extern void init_bottom(void);

#define lockb_timer(flags) lock_bottom(TIMER_B, &flags)
#define lockb_ide(flags) lock_bottom(IDE_B, &flags)
#define lockb_kbd(flags) lock_bottom(KEYBOARD_B, &flags)
#define lockb_flp(flags) lock_bottom(FLOOPY_B, &flags)
#define lockb_all(flags) lock_all_bottom(&flags)
#define saveb_flags(flags) save_bottom_mask(&flags)

#define unlockb_timer(flags) unlock_bottom(TIMER_B, flags)
#define unlockb_ide(flags) unlock_bottom(IDE_B, flags)
#define unlockb_kbd(flags) unlock_bottom(KEYBOARD_B, flags)
#define unlockb_flp(flags) unlock_bottom(FLOOPY_B, flags)
#define unlockb_all(flags) unlock_all_bottom(flags)
#define restoreb_flags(flags) restore_bottom_mask(flags)

#define stib() unlock_all_bottom(0x0)

#endif
