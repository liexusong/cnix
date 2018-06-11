#ifndef _KEYMAP_H
#define _KEYMAP_H

#define EXT	0x0100
#define CAPS	0x0200

#define	C(c)	((c) & 0x1F)
#define A(c)	((c) | 0x80)
#define CA(c)	A(C(c))	
#define	L(c)	((c) | CAPS)

#define EXTEND(x)	(x + EXT)

#define HOME	EXTEND(0x01)
#define END	EXTEND(0x02)
#define UP	EXTEND(0x03)
#define DOWN	EXTEND(0x04)
#define LEFT	EXTEND(0x05)
#define RIGHT	EXTEND(0x06)
#define PGUP	EXTEND(0x07)
#define PGDN	EXTEND(0x08)
#define MIDDLE	EXTEND(0x09)
#define MINUS	EXTEND(0x0a)
#define PLUS	EXTEND(0x0b)
#define INSERT	EXTEND(0x0c)

#define CLOCK	EXTEND(0x0d)
#define	NLOCK	EXTEND(0x0e)
#define SLOCK	EXTEND(0x0f)

#define F1	EXTEND(0x10)
#define F2	EXTEND(0x11)
#define F3	EXTEND(0x12)
#define F4	EXTEND(0x13)
#define F5	EXTEND(0x14)
#define F6	EXTEND(0x15)
#define F7	EXTEND(0x16)
#define F8	EXTEND(0x17)
#define F9	EXTEND(0x18)
#define F10	EXTEND(0x19)
#define F11	EXTEND(0x1a)
#define F12	EXTEND(0x1b)

#define AHOME	EXTEND(0x1c)
#define AEND	EXTEND(0x1d)
#define AUP	EXTEND(0x1e)
#define ADOWN	EXTEND(0x1f)
#define ALEFT	EXTEND(0x20)
#define ARIGHT	EXTEND(0x21)
#define APGUP	EXTEND(0x22)
#define APGDN	EXTEND(0x23)
#define AMIDDLE	EXTEND(0x24)
#define AMINUS	EXTEND(0x25)
#define APLUS	EXTEND(0x26)
#define AINSERT	EXTEND(0x27)

#define AF1	EXTEND(0x28)
#define AF2	EXTEND(0x29)
#define AF3	EXTEND(0x2a)
#define AF4	EXTEND(0x2b)
#define AF5	EXTEND(0x2c)
#define AF6	EXTEND(0x2d)
#define AF7	EXTEND(0x2e)
#define AF8	EXTEND(0x2f)
#define AF9	EXTEND(0x30)
#define AF10	EXTEND(0x31)
#define AF11	EXTEND(0x32)
#define AF12	EXTEND(0x33)

#define CSPACE	230
#define SSPACE	231
#define CSHIFT	233

#define CHOME	EXTEND(0x34)
#define CEND	EXTEND(0x35)
#define CUP	EXTEND(0x36)
#define CDOWN	EXTEND(0x37)
#define CLEFT	EXTEND(0x38)
#define CRIGHT	EXTEND(0x39)
#define CPGUP	EXTEND(0x3a)
#define CPGDN	EXTEND(0x3b)
#define CMIDDLE	EXTEND(0x3c)
#define CMINUS	EXTEND(0x3d)
#define CPLUS	EXTEND(0x3e)
#define CINSERT	EXTEND(0x3f)

#define CF1	EXTEND(0x40)
#define CF2	EXTEND(0x41)
#define CF3	EXTEND(0x42)
#define CF4	EXTEND(0x43)
#define CF5	EXTEND(0x44)
#define CF6	EXTEND(0x45)
#define CF7	EXTEND(0x46)
#define CF8	EXTEND(0x47)
#define CF9	EXTEND(0x48)
#define CF10	EXTEND(0x49)
#define CF11	EXTEND(0x4a)
#define CF12	EXTEND(0x4b)

#define SF1	EXTEND(0x4c)
#define SF2	EXTEND(0x4d)
#define SF3	EXTEND(0x4e)
#define SF4	EXTEND(0x4f)
#define SF5	EXTEND(0x50)
#define SF6	EXTEND(0x51)
#define SF7	EXTEND(0x52)
#define SF8	EXTEND(0x53)
#define SF9	EXTEND(0x54)
#define SF10	EXTEND(0x55)
#define SF11	EXTEND(0x56)
#define SF12	EXTEND(0x57)

#define ASF1	EXTEND(0x58)
#define ASF2	EXTEND(0x59)
#define ASF3	EXTEND(0x5a)
#define ASF4	EXTEND(0x5b)
#define ASF5	EXTEND(0x5c)
#define ASF6	EXTEND(0x5d)
#define ASF7	EXTEND(0x5e)
#define ASF8	EXTEND(0x5f)
#define ASF9	EXTEND(0x60)
#define ASF10	EXTEND(0x61)
#define ASF11	EXTEND(0x62)
#define ASF12	EXTEND(0x63)

#define SHIFT	EXTEND(0x64)
#define CTRL	EXTEND(0x65)
#define ALT	EXTEND(0x66)

#define SPGUP	EXTEND(0x67)
#define SPGDOWN	EXTEND(0x68)

#endif