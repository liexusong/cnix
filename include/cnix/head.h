#ifndef _HEAD_H
#define _HEAD_H

typedef struct desc_struct{
	unsigned long a, b;
}desc_table[256];

extern unsigned long kp_dir[];
extern desc_table gdt, idt;

extern unsigned long _text;
extern unsigned long _etext;
extern unsigned long _data;
extern unsigned long _edata;
extern unsigned long _bss;
extern unsigned long _ebss;
extern unsigned long _end;

#endif
