#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
#define	DEF_MIN		0
#define	DEF_TIME	0

struct termios def_termios = {
	(BRKINT | ICRNL | IXON | IXANY),			/*c_iflag*/
	(OPOST | ONLCR),					/* c_oflag */
	(CREAD | CS8 | HUPCL),					/* c_cflag */
	(ISIG | IEXTEN | ICANON | ECHO | ECHOE | ECHOCTL),	/* c_lflag */
	0,
	{							/* c_cc */
		DEF_INTR,
		DEF_QUIT,
		DEF_ERASE,
		DEF_KILL,
		DEF_EOF,
		DEF_TIME,
		DEF_MIN,
		DEF_STATUS, //XXXX////
		DEF_START,
		DEF_STOP,
		DEF_DSUSP,
		DEF_EOL,
		DEF_REPRINT,
		DEF_DISCARD, 
		DEF_WERASE,
		DEF_LNEXT,
		DEF_EOL2	
	},
	0xffffffff,
	0xffffffff,
};

void reset(char * ttyname)
{
	int fd;

	fd = open(ttyname, O_RDWR, 0);
	if(fd < 0){
		printf("open %s error\n", ttyname);
		_exit(0);
	}

	tcsetattr(fd, TCSANOW, &def_termios);

	close(fd);
}

int main(int argc, char * argv[], char * envp[])
{
	if(argc < 2){
		printf("retty ttyname\n");
		_exit(0);
	}

	printf("reset %s\n", argv[1]);

	reset(argv[1]);

	return 0;
}
