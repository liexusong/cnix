#include <stdio.h>
#include "../include/cnix/keymap.h"

/* from minix */
unsigned short key_map[128 * 6] = {
	0,	0,	0,	0,	0,	0,
	C('['),	C('['),	CA('['),CA('['),CA('['),C('['),
	'1',	'!',	A('1'),	A('1'),	A('!'),	C('A'),
	'2',	'@',	A('2'),	A('2'),	A('@'),	C('@'),
	'3',	'#',	A('3'),	A('3'),	A('#'),	C('C'),
	'4',	'$',	A('4'),	A('4'),	A('$'),	C('D'),
	'5',	'%',	A('5'),	A('5'),	A('%'),	C('E'),
	'6',	'^',	A('6'),	A('6'),	A('^'),	C('^'),
	'7',	'&',	A('7'),	A('7'),	A('&'),	C('G'),
	'8',	'*',	A('8'),	A('8'),	A('*'),	C('H'),
	'9',	'(',	A('9'),	A('9'),	A('('),	C('I'),
	'0',	')',	A('0'),	A('0'),	A(')'),	C('@'),
	'-',	'_',	A('-'),	A('-'),	A('_'),	C('_'),
	'=',	'+',	A('='),	A('='),	A('+'),	C('@'),
	C('H'),	C('H'),	CA('H'),CA('H'),CA('H'),0177,	
	C('I'),	C('I'),	CA('I'),CA('I'),CA('I'),C('I'),
	L('q'),	'Q',	A('q'),	A('q'),	A('Q'),	C('Q'),
	L('w'),	'W',	A('w'),	A('w'),	A('W'),	C('W'),
	L('e'),	'E',	A('e'),	A('e'),	A('E'),	C('E'),
	L('r'),	'R',	A('r'),	A('r'),	A('R'),	C('R'),
	L('t'),	'T',	A('t'),	A('t'),	A('T'),	C('T'),
	L('y'),	'Y',	A('y'),	A('y'),	A('Y'),	C('Y'),
	L('u'),	'U',	A('u'),	A('u'),	A('U'),	C('U'),
	L('i'),	'I',	A('i'),	A('i'),	A('I'),	C('I'),
	L('o'),	'O',	A('o'),	A('o'),	A('O'),	C('O'),
	L('p'),	'P',	A('p'),	A('p'),	A('P'),	C('P'),
	'[',	'{',	A('['),	A('['),	A('{'),	C('['),
	']',	'}',	A(']'),	A(']'),	A('}'),	C(']'),
	C('M'),	C('M'),	CA('M'),CA('M'),CA('M'),C('J'),
	CTRL,	CTRL,	CTRL,	CTRL,	CTRL,	CTRL,
	L('a'),	'A',	A('a'),	A('a'),	A('A'),	C('A'),
	L('s'),	'S',	A('s'),	A('s'),	A('S'),	C('S'),
	L('d'),	'D',	A('d'),	A('d'),	A('D'),	C('D'),
	L('f'),	'F',	A('f'),	A('f'),	A('F'),	C('F'),
	L('g'),	'G',	A('g'),	A('g'),	A('G'),	C('G'),
	L('h'),	'H',	A('h'),	A('h'),	A('H'),	C('H'),
	L('j'),	'J',	A('j'),	A('j'),	A('J'),	C('J'),
	L('k'),	'K',	A('k'),	A('k'),	A('K'),	C('K'),
	L('l'),	'L',	A('l'),	A('l'),	A('L'),	C('L'),
	';',	':',	A(';'),	A(';'),	A(':'),	C('@'),
	'\'',	'"',	A('\''),A('\''),A('"'),	C('@'),
	'`',	'~',	A('`'),	A('`'),	A('~'),	C('@'),
	SHIFT,	SHIFT,	SHIFT,	SHIFT,	SHIFT,	SHIFT,
	'\\',	'|',	A('\\'),A('\\'),A('|'),	C('\\'),
	L('z'),	'Z',	A('z'),	A('z'),	A('Z'),	C('Z'),
	L('x'),	'X',	A('x'),	A('x'),	A('X'),	C('X'),
	L('c'),	'C',	A('c'),	A('c'),	A('C'),	C('C'),
	L('v'),	'V',	A('v'),	A('v'),	A('V'),	C('V'),
	L('b'),	'B',	A('b'),	A('b'),	A('B'),	C('B'),
	L('n'),	'N',	A('n'),	A('n'),	A('N'),	C('N'),
	L('m'),	'M',	A('m'),	A('m'),	A('M'),	C('M'),
	',',	'<',	A(','),	A(','),	A('<'),	C('@'),
	'.',	'>',	A('.'),	A('.'),	A('>'),	C('@'),
	'/',	'?',	A('/'),	A('/'),	A('?'),	C('@'),
//	SHIFT,	SHIFT,	SHIFT,	SHIFT,	SHIFT,	SHIFT,
	SHIFT,	SHIFT,	SHIFT,	SHIFT,	SHIFT,	CSHIFT,
	'*',	'*',	A('*'),	A('*'),	A('*'),	C('@'),
	ALT,	ALT,	ALT,	ALT,	ALT,	ALT,
//	' ',	' ',	A(' '),	A(' '),	A(' '),	C('@'),
	' ',	SSPACE,	A(' '),	A(' '),	A(' '),	CSPACE,
	CLOCK,	CLOCK,	CLOCK,	CLOCK,	CLOCK,	CLOCK,
	F1,	SF1,	AF1,	AF1,	ASF1,	CF1,
	F2,	SF2,	AF2,	AF2,	ASF2,	CF2,
	F3,	SF3,	AF3,	AF3,	ASF3,	CF3,
	F4,	SF4,	AF4,	AF4,	ASF4,	CF4,
	F5,	SF5,	AF5,	AF5,	ASF5,	CF5,
	F6,	SF6,	AF6,	AF6,	ASF6,	CF6,
	F7,	SF7,	AF7,	AF7,	ASF7,	CF7,
	F8,	SF8,	AF8,	AF8,	ASF8,	CF8,
	F9,	SF9,	AF9,	AF9,	ASF9,	CF9,
	F10,	SF10,	AF10,	AF10,	ASF10,	CF10,
	NLOCK,	NLOCK,	NLOCK,	NLOCK,	NLOCK,	NLOCK,
	SLOCK,	SLOCK,	SLOCK,	SLOCK,	SLOCK,	SLOCK,
	HOME,	'7',	AHOME,	AHOME,	A('7'),	CHOME,	
	UP,	'8',	AUP,	AUP,	A('8'),	CUP,
	PGUP,	0x167/*'9'*/,	APGUP,	APGUP,	A('9'),	CPGUP,
	MINUS,	'-',	AMINUS,	AMINUS,	A('-'),	CMINUS,
	LEFT,	'4',	ALEFT,	ALEFT,	A('4'),	CLEFT,
	MIDDLE,	'5',	AMIDDLE,AMIDDLE,A('5'),	CMIDDLE,
	RIGHT,	'6',	ARIGHT,	ARIGHT,	A('6'),	CRIGHT,
	PLUS,	'+',	APLUS,	APLUS,	A('+'),	CPLUS,
	END,	'1',	AEND,	AEND,	A('1'),	CEND,
	DOWN,	'2',	ADOWN,	ADOWN,	A('2'),	CDOWN,
	PGDN,	0x168/*'3'*/,	APGDN,	APGDN,	A('3'),	CPGDN,
	INSERT,	'0',	AINSERT,AINSERT,A('0'),	CINSERT,
	0177,	'.',	A(0177),A(0177),A('.'),	0177,
	C('M'),	C('M'),	CA('M'),CA('M'),CA('M'),C('J'),
	0,	0,	0,	0,	0,	0,
	'<',	'>',	A('<'),	A('|'),	A('>'),	C('@'),
	F11,	SF11,	AF11,	AF11,	ASF11,	CF11,
	F12,	SF12,	AF12,	AF12,	ASF12,	CF12,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	EXT,	EXT,	EXT,	EXT,	EXT,	EXT,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0
};

void print_hex(int j)
{
	char c;

	c = j + '0';
	if(c > '9')
		c += ('a' - '0') - 10;
	printf("%c", c);
}

void print_map(char * string, unsigned short * ch)
{
	int i, j;

	printf("static unsigned short %s[NR_KEYS] = {", string);
	for(i = 0; i < 128; i++, ch += 6){
		if(!(i & 7))
			printf("\n");
		printf("\t0x");
		print_hex(((*ch) >> 12) & 0x0f);
		print_hex(((*ch) >> 8) & 0x0f);
		print_hex(((*ch) >> 4) & 0x0f);
		print_hex((*ch) & 0x0f);
		printf(",");
	}
	printf("\n};\n\n");
}

int main(void)
{
	unsigned short * map;

	map = key_map;
	printf("#include <cnix/types.h>\n\n");
	printf("#define NR_KEYS 128\n\n");
	print_map("plain", map++);
	print_map("shift", map++);
	print_map("alt", map++);
	print_map("altgr", map++);
	print_map("altsh", map++);
	print_map("ctrl", map);
	printf("/* none, shift, ctrl, lalt, alt(lalt | ralt) + shift, ctrl + ralt */\n");
	printf("unsigned short * key_maps[] = {\n");
	printf("\tplain,	shift,	ctrl,	NULL,\n");
	printf("\talt,	altsh,	NULL,	NULL,\n");
	printf("\talt, 	altsh,	altgr,	NULL,\n");
	printf("\tNULL,	NULL,	NULL,	NULL\n");
	printf("};\n");
	return 0;	
}
