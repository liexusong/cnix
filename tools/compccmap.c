#include <stdio.h>

#define	CR		'\r'
#define	NL		'\n'
#define	DEF_DISCARD 	'\17'	/* ^O */
#define	DEF_DSUSP	'\31'	/* ^Y */
#define	DEF_EOF 	'\4'	/* ^D */
#define	DEF_EOL 	0xff
#define	DEF_EOL2 	0xff
#define	DEF_ERASE	'\10'	/* ^H */
#define	DEF_INTR 	'\3'	/* ^C */
#define	DEF_KILL 	'\25'	/* ^U */
#define	DEF_LNEXT	'\26'	/* ^V */
#define	DEF_QUIT	'\34'	/* ^\ */
#define	DEF_REPRINT	'\22'	/* ^R */
#define	DEF_START	'\21'	/* ^Q */
#define	DEF_STATUS	'\24'	/* ^T */
#define	DEF_STOP	'\23'	/* ^S */
#define	DEF_SUSP	'\32'	/* ^Z */
#define	DEF_WERASE	'\27'	/* ^W */

unsigned char cc[] = {
	CR,
	NL,
	DEF_DISCARD, 
	DEF_DSUSP,
	DEF_EOF,
	DEF_EOL,
	DEF_EOL2,
	DEF_ERASE,
	DEF_INTR,
	DEF_KILL,
	DEF_LNEXT,
	DEF_QUIT,
	DEF_REPRINT,
	DEF_START,
	DEF_STATUS,
	DEF_STOP,
	DEF_SUSP,
	DEF_WERASE,
};

int main(void)
{
	int i;
	unsigned long ccmap[4];

	ccmap[0] = ccmap[1] = ccmap[2] = ccmap[3] = 0;
	for(i = 0; i < sizeof(cc); i++){
		if(cc[i] > 127)
			continue;

		ccmap[cc[i] / 32] |= 1 << (cc[i] % 32);
	}
	
	printf("%x, %x, %x, %x\n", ccmap[0], ccmap[1], ccmap[2], ccmap[3]);
}

