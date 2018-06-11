#ifndef _STRING_H
#define _STRING_H

#define memcpy kmemcpy
#define memset kmemset
#define strlen kstrlen
#define strcmp kstrcmp
#define strncmp kstrncmp
#define strcpy kstrcpy
#define strncpy kstrncpy
#define strcat kstrcat
#define strncat kstrncat
#define strchr kstrchr
#define strrchr kstrrchr
#define strstr kstrstr

static inline void kmemcpy(void * dest, const void * src, int n)
{
	int D, S, c;

	__asm__ __volatile__("cld\n\t"
		"rep; movsl\n\t"
		"testb $2, %b6\n\t"
		"je 1f\n\t"
		"movsw\n"
		"1:\ttestb $1, %b6\n\t"
		"je 2f\n\t"
		"movsb\n"
		"2:"
		:"=&D"(D), "=&S"(S), "=&c"(c)
		:"0"(dest), "1"(src), "2"(n / 4), "q"(n)
		:"memory"
	);
}

static inline void kmemset(void * dest, int ch, int n)
{
	int D, c;

	__asm__ __volatile__("cld\n\t"
		"rep; stosl\n\t"
		"testb $2, %b5\n\t"
		"je 1f\n\t"
		"stosw\n"
		"1:\ttestb $1, %b5\n\t"
		"je 2f\n\t"
		"stosb\n"
		"2:"
		:"=&D"(D), "=&c"(c)
		:"0"(dest), "1"(n / 4), 
		 "a"(ch | ch << 8 | ch << 16 | ch << 24), "q"(n)
		:"memory"
	);
}

static inline int kstrlen(const char * str)
{
	int D, c, a, ret;

	__asm__ __volatile__("cld\n"
		"repne; scasb\n\t"
		"subl %1, %2\n\t"
		:"=&D"(D), "=&c"(c), "=q"(ret), "=&a"(a)
		:"0"(str), "1"(0xffffffff), "2"(0xfffffffe), "3"(0)
		:"memory"
	);

	return ret;
}

static inline int kstrcmp(const char * s1, const char * s2)
{
	int S, D, a;
	
	__asm__ __volatile__("cld\n"
		"1:\tlodsb\n\t"
		"scasb\n\t"
		"jne 2f\n\t"
		"testb %%al, %%al\n\t"
		"jne 1b\n\t"
		"xorl %2, %2\n\t"
		"jmp 3f\n"
		"2:\tsbbl %2, %2\n"
		"orb $1, %%al\n"
		"3:"
		:"=S"(S), "=D"(D), "=a"(a)
		:"0"(s1), "1"(s2)
		:"memory"
	);

	return a;
}

static inline int kstrncmp(const char * s1, const char * s2, int n)
{
	int S, D, c, a;
	
	__asm__ __volatile__("cld\n"
		"1:\tdecl %3\n\t"
		"js 4f\n\t"
		"lodsb\n\t"
		"scasb\n\t"
		"jne 2f\n\t"
		"testb %%al, %%al\n\t"
		"jne 1b\n"
		"4:\txorl %2, %2\n\t"
		"jmp 3f\n"
		"2:\tsbbl %2, %2\n"
		"orb $1, %%al\n"
		"3:"
		:"=S"(S), "=D"(D), "=a"(a), "=c"(c)
		:"0"(s1), "1"(s2), "3"(n)
		:"memory"
	);

	return a;
}

static inline char * kstrcpy(char * dest, const char * src)
{
	int D, S, a;
	
	__asm__ __volatile__("cld\n\t"
		"1:\tlodsb\n\t"
		"stosb\n\t"
		"testb %%al, %%al\n\t"
		"jne 1b\n"
		"2:"
		:"=D"(D), "=S"(S), "=a"(a)
		:"0"(dest), "1"(src)
		:"memory"
	);

	return dest;
}

static inline char * kstrncpy(char * dest, const char * src, int n)
{
	int D, S, c, a;
	
	__asm__ __volatile__("cld\n\t"
		"1:\tdecl %2\n\t"
		"js 3f\n\t"
		"lodsb\n\t"
		"stosb\n\t"
		"testb %%al, %%al\n\t"
		"jne 1b\n\t"
		"3:\tmovb $0, (%%edi)\n"
		"2:"
		:"=D"(D), "=S"(S), "=c"(c), "=a"(a)
		:"0"(dest), "1"(src), "2"(n)
		:"memory"
	);

	return dest;
}

static inline char * kstrcat(char * dest, const char * src)
{
	int D, S, c, a;

	__asm__ __volatile__("cld\n\t"
		"repne; scasb\n\t"
		"decl %0\n\t"
		"1:\tlodsb\n\t"
		"stosb\n\t"
		"testb %%al, %%al\n\t"
		"jne 1b"
		:"=D"(D), "=S"(S), "=c"(c), "=a"(a)
		:"0"(dest), "1"(src), "2"(0xffffffff), "3"(0)
		:"memory"
	);

	return dest;
}


static inline char * kstrncat(char * dest, const char * src, int n)
{
	int D, S, c, a;

	__asm__ __volatile__("cld\n\t"
		"repne; scasb\n\t"
		"decl %0\n\t"
		"movl %7, %2\n\t"
		"1:\ttestl %2, %2\n\t"
		"je 2f\n\t"
		"decl %2\n\t"
		"lodsb\n\t"
		"stosb\n\t"
		"testb %%al, %%al\n\t"
		"jne 1b\n"
		"2:\tmovb $0, (%%edi)"
		:"=&D"(D), "=&S"(S), "=&c"(c), "=&a"(a)
		:"0"(dest), "1"(src), "2"(0xffffffff), "3"(0), "q"(n)
		:"memory"
	);

	return dest;
}

static inline char * kstrchr(const char * s, int c)
{
	int a;
	char * S;

	__asm__ __volatile__("cld\n\t"
		"movb %%al, %%ah\n"
		"1:\tlodsb\n\t"
		"cmp %%al, %%ah\n\t"
		"je 2f\n\t"
		"testb %%al, %%al\n\t"
		"jne 1b\n\t"
		"3:\tmovl $1, %0\n"
		"2:decl %0"
		:"=S"(S), "=a"(a)
		:"0"(s), "1"(c)
		:"memory"
	);

	return S;
}

static inline char * kstrrchr(const char * s, int c)
{
	int a;
	char * S, * D;

	__asm__ __volatile__("cld\n\t"
		"movb %%al, %%ah\n\t"
		"1:\tlodsb\n\t"
		"cmp %%al, %%ah\n\t"
		"jne 2f\n\t"
		"movl %0, %1\n"
		"2:\ttest %%al, %%al\n\t"
		"jne 1b\n\t"
		"decl %1"
		:"=S"(S), "=D"(D), "=a"(a)
		:"0"(s), "1"(1), "2"(c)
		:"memory"
	);

	return D;
}

static inline char * kstrstr(const char * haystack, const char * needle)
{
	int c, d;
	char * a, * S, * D;

	__asm__ __volatile__("cld\n\t"
		"repne; scasb\n\t"
		"notl %3\n\t"
		"decl %3\n\t"
		"movl %3, %4\n"
		"1:\tmovl %4, %3\n\t"
		"movl %0, %2\n\t"
		"movl %9, %1\n\t"
		"repe; cmpsb\n\t"
		"je 2f\n\t"
		"xchgl %2, %0\n\t"
		"incl %0\n\t"
		"cmpb $0, -1(%%eax)\n\t"
		"jne 1b\n\t"
		"xorl %2, %2\n"
		"2:"
		:"=&S"(S), "=&D"(D), "=&a"(a), "=&c"(c), "=&d"(d)
		:"0"(haystack), "1"(needle), "2"(0), "3"(0xffffffff), "g"(needle)
		:"memory"
	);

	return a;
}

#endif
