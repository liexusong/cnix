#ifndef _IO_H
#define _IO_H

#define outb(value,port) \
__asm__ __volatile__("outb %%al,%%dx"::"a" (value),"d" (port))


#define inb(port) ({ \
unsigned char _v; \
__asm__ __volatile__("inb %%dx,%%al":"=a" (_v):"d" (port)); \
_v; \
})

#define outb_p(value,port) \
__asm__ __volatile__("outb %%al,%%dx\n" \
		"\tjmp 1f\n" \
		"1:\tjmp 1f\n" \
		"1:"::"a" (value),"d" (port))

#define inb_p(port) ({ \
unsigned char _v; \
__asm__ __volatile__ ("inb %%dx,%%al\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:":"=a" (_v):"d" (port)); \
_v; \
})

#define outw(value,port) \
__asm__ __volatile__("outw %%ax,%%dx"::"a" (value),"d" (port))


#define inw(port) ({ \
unsigned short _v; \
__asm__ __volatile__("inw %%dx,%%ax":"=a" (_v):"d" (port)); \
_v; \
})

#define outw_p(value,port) \
__asm__ __volatile__("outw %%ax,%%dx\n" \
		"\tjmp 1f\n" \
		"1:\tjmp 1f\n" \
		"1:"::"a" (value),"d" (port))

#define inw_p(port) ({ \
unsigned short _v; \
__asm__ __volatile__ ("inw %%dx,%%ax\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:":"=a" (_v):"d" (port)); \
_v; \
})

#define outl(value,port) \
__asm__ __volatile__("outl %%eax,%%dx"::"a" (value),"d" (port))


#define inl(port) ({ \
unsigned long _v; \
__asm__ __volatile__("inl %%dx,%%eax":"=a" (_v):"d" (port)); \
_v; \
})

#define outl_p(value,port) \
__asm__ __volatile__("outl %%eax,%%dx\n" \
		"\tjmp 1f\n" \
		"1:\tjmp 1f\n" \
		"1:"::"a" (value),"d" (port))

#define inl_p(port) ({ \
unsigned long _v; \
__asm__ __volatile__ ("inl %%dx,%%eax\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:":"=a" (_v):"d" (port)); \
_v; \
})

#endif
