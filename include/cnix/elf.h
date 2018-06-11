#ifndef _ELF_H
#define _ELF_H

/* values for elf_phdr.p_type */
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_LOPROC  0x70000000
#define PT_HIPROC  0x7fffffff

/* values for elf_phdr.p_flags */
#define PF_R	0x4
#define PF_W	0x2
#define PF_X	0x1

/* values for elfhdr.e_type */
#define ET_NONE   0
#define ET_REL    1
#define ET_EXEC   2
#define ET_DYN    3
#define ET_CORE   4
#define ET_LOPROC 5
#define ET_HIPROC 6

/* values for elfhdr.e_machine */
#define EM_386   3

struct elfhdr{
	unsigned char e_ident[16];
	unsigned short  e_type;
	unsigned short e_machine;
	unsigned long   e_version;
	unsigned long e_entry; 
	unsigned long e_phoff;
	unsigned long e_shoff;
	unsigned long e_flags;
	unsigned short e_ehsize;
	unsigned short e_phentsize;
	unsigned short e_phnum;
	unsigned short e_shentsize;
	unsigned short e_shnum;
	unsigned short e_shstrndx;
};

struct elf_phdr{
	unsigned long p_type;
	unsigned long p_offset;
	unsigned long p_vaddr;
	unsigned long p_paddr;
	unsigned long p_filesz;
	unsigned long p_memsz;
	unsigned long p_flags;
	unsigned long p_align;
};

#endif
