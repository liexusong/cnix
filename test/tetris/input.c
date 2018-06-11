#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "screen.h"
#include "input.h"
#include "utils.h"


static struct termios backup_term;

extern  debug_file_t *debugfile;

enum
{
	RESET,
	RAW,
	CBREAK
};

int tty_state = RESET;
int saved_fd = -1;

int tty_cbreak(int fd)
{
	struct termios buff;

	if (tcgetattr(fd, &backup_term) < 0)
	{
		return -1;
	}

	buff = backup_term;

	buff.c_lflag &= ~(ICANON | ECHO);

	buff.c_cc[VMIN] = 1;
	buff.c_cc[VTIME] = 0;

	if (tcsetattr(fd, TCSAFLUSH, &buff) < 0)
	{
		return -1;
	}

	saved_fd = fd;
	tty_state = CBREAK;

	return 0;
}

///
int tty_raw(int fd)
{
	struct termios tm;
	
	if (tcgetattr(0, &backup_term) < 0)
	{
		return -1;
	}

	tm = backup_term;

	tm.c_lflag &= ~(ICANON | ECHO | IEXTEN | ISIG);
	tm.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	tm.c_cflag &= ~(CSIZE | PARENB);
	tm.c_cflag |= CS8;
	tm.c_oflag &= ~(OPOST);
	
	tm.c_cc[VMIN] = 1;
	tm.c_cc[VTIME] = 0;

	if (tcsetattr(0, TCSAFLUSH, &tm) < 0)
	{
		return -1;
	}

	saved_fd = fd;
	tty_state = RAW;

	return 0;
	
}


///
int tty_reset(int fd)
{
	if (tty_state != CBREAK && tty_state != RAW)
	{
		return 0;
	}

	if (tcsetattr(fd, TCSAFLUSH, &backup_term) < 0)
	{
		return -1;
	}

	tty_state = RESET;

	return 0;
}


void sig_catcher(int val)
{
	printf("signal caught\n");
	
	tty_reset(STDIN_FILENO);

	exit(0);
}


/*----------------------------------------------------------------------
     Lowest level read command. This reads one character with timeout. (UNIX)

    Args:  time_out --  number of seconds before read will timeout

  Result: Returns a single character read or a NO_OP_COMMAND if the
          timeout expired, or a KEY_RESIZE if a resize even occured.

  ----*/
int
read_with_timeout(int time_out)
{
#if 0
#ifdef USE_POLL
	struct pollfd pollfd;
#else
	struct timeval tmo;
	fd_set         readfds, errfds;
#endif
#endif
	int res;
	unsigned char  c = 0;


	fflush(stdout);

	if(time_out > 0)
	{
#if 0	
		 /* Check to see if there's bytes to read with a timeout */
#ifdef USE_POLL
		pollfd.fd = STDIN_FD;
		pollfd.events = POLLIN;
		dprint(debugfile, 9, "poll event %d, timeout %d\n", pollfd.events,
			time_out));
		res = poll (&pollfd, 1, time_out * 1000);
		dprint(debugfile, 9, "poll on tty returned %d, events %d\n",
				res, pollfd.revents));
#else
		 FD_ZERO(&readfds);
		 FD_ZERO(&errfds);
		 FD_SET(STDIN_FILENO, &readfds);
		 FD_SET(STDIN_FILENO, &errfds);
		 tmo.tv_sec  = time_out;
		 tmo.tv_usec = 0;

		 dprint(debugfile, 9, "Select readfds:%d timeval:%d,%d\n", readfds,
			   tmo.tv_sec,tmo.tv_usec);

		 res = select(STDIN_FILENO + 1, &readfds, 0, &errfds, &tmo);

		 dprint(debugfile, 9, "Select on tty returned %d\n", res);
#endif

		if(res < 0) 
		{
			if (errno == EINTR) 
			{
				return (NO_OP_COMMAND);
			}
			//panic1("Select error: %s\n", error_description(errno));
			exit(1);
		}

		if(res == 0) 
		{ 	/* the select timed out */
			return(time_out > 25 ? NO_OP_IDLE: NO_OP_COMMAND);
		}
#endif		
	}

	res = read(STDIN_FILENO, &c, 1);
	dprint(debugfile, 8,  "Read returned %c\n", res);

	if (res < 0) 
	{
		/* Got an error reading from the terminal. Treat this like
		a SIGHUP: clean up and exit. */
		dprint(debugfile, 1,  "\n\n** Error reading from tty :\n\n");
		
		if (errno == EINTR)
			return (NO_OP_COMMAND);

#ifndef CNIX
		dprint(debugfile, 1, "errno = %d, errmsg = %s\n", errno, strerror(errno));
#endif
	
	//	exit(0);
		return NO_OP_COMMAND;
	}

	return ((int)c);
}


#if 0
int read_char(int timeout)
{
	int cCh, iFlag = 0;

	for(;;){
		cCh = getchar();

		if(cCh != (unsigned short)-1){
			switch(cCh){
			case 27: /* prefix of UP DOWN LEFT RIGHT */
			case 91:
				iFlag++;
				break;
			case 65: //UP
			case 66: //DOWN
			case 67: //RIGHT
			case 68: //LEFT
				if(iFlag == 2)
					return (cCh | 0x100);
			default:
				return cCh;
			}
		}
	}
}

#else
/*----------------------------------------------------------------------
  Read input characters with lots of processing for arrow keys and such  (UNIX)

 Args:  time_out -- The timeout to for the reads
        char_in     -- Input character that needs processing

 Result: returns the character read. Possible special chars defined h file

    This deals with function and arrow keys as well.

  The idea is that this routine handles all escape codes so it done in
  only one place. Especially so the back arrow key can work when entering
  things on a line. Also so all function keys can be disabled and not
  cause weird things to happen.


  Assume here that any chars making up an escape sequence will be close
  together over time. It's possible for a timeout to occur waiting for rest
  of escape sequence if it takes more than 30 seconds to type the
  escape sequence. The timeout will effectively cancel the escape sequence.

  ---*/
int read_char(int time_out)
{
	register int  rx, ch, num_keys;
	
	rx = 0; /* avoid ref before set errors */
	ch = read_with_timeout(time_out);
	if(ch == NO_OP_IDLE || ch == NO_OP_COMMAND || ch == KEY_RESIZE)
		goto done;
	ch &= 0x7f;
	switch (ch) 
	{
		case '\033':
			ch = read_with_timeout(time_out);
			if(ch == NO_OP_IDLE || ch == NO_OP_COMMAND || ch == KEY_RESIZE)
				goto done;
			ch &= 0x7f;
			
			if (ch == 'O')
			{
				/* For DEC terminals, vt100s */
				ch = read_with_timeout(time_out);
				if(ch == NO_OP_IDLE || ch == NO_OP_COMMAND || ch == KEY_RESIZE)
					goto done;
				ch &= 0x7f;
				switch (ch)
				{
					case 'P': 
						return(PF1);
					case 'Q': 
						return(PF2);
					case 'R': 
						return(PF3);
					case 'S':
						return(PF4);
					case 'p': 
						return(PF5);
					case 'q':
						return(PF6);
					case 'r':
						return(PF7);
					case 's': 
						return(PF8);
					case 't': 
						return(PF9);
					case 'u':
						return(PF10);
					case 'v': 
						return(PF11);
					case 'w':
						return(PF12);
					case 'A':
						return(KEY_UP);
					case 'B':
						return(KEY_DOWN);
					case 'C': 
						return(KEY_RIGHT);
					case 'D':
						return(KEY_LEFT);
					/* default: return junk below */
				}
			} 
			else if (ch == '[')
			{
				/* For dec terminals, vt200s, and some weird Sun stuff */
				ch = read_with_timeout(time_out);
				if(ch == NO_OP_IDLE || ch == NO_OP_COMMAND || ch == KEY_RESIZE)
					goto done;
				
				ch &= 0x7f;
				switch (ch) 
				{
					case 'A': 
						return(KEY_UP);
					case 'B': 
						return(KEY_DOWN);
					case 'C':
						return(KEY_RIGHT);
					case 'D': 
						return(KEY_LEFT);

					case '=': /* ansi terminal function keys */
						ch = read_with_timeout(time_out);
						if(ch == NO_OP_IDLE || ch == NO_OP_COMMAND || ch == KEY_RESIZE)
							goto done;
						
						ch &= 0x7f;
						switch(ch) 
						{
							case 'a':
								return(PF1);
							case 'b': 
								return(PF2);
							case 'c': 
								return(PF3);
							case 'd': 
								return(PF4);
							case 'e': 
								return(PF5);
							case 'f': 
								return(PF6);
							case 'g': 
								return(PF7);
							case 'h': 
								return(PF8);
							case 'i': 
								return(PF9);
							case 'j': 
								return(PF10);
							case 'k': 
								return(PF11);
							case 'l':
								return(PF12);
							/* default: return junk below */
						}
						break;

				case '1': /* Sun keys */
					rx = KEY_JUNK; 
					goto swallow_till_z;

				case '2': /* Sun keys */
					ch = read_with_timeout(2);
					if(ch == NO_OP_IDLE || ch == NO_OP_COMMAND)
						return(KEY_JUNK);
					if(ch == KEY_RESIZE)
						goto done;
					
					ch &= 0x7f;
					if(ch == '1') 
					{
						ch = read_with_timeout(2);
						if(ch == NO_OP_IDLE || ch == NO_OP_COMMAND)
							return(KEY_JUNK);
						
						if(ch == KEY_RESIZE)
							goto done;
						
						ch &= 0x7f;
						switch (ch) 
						{
							case '5': 
								rx = KEY_UP;
								break;
								
							case '7': 
								rx = KEY_LEFT;
								break;
								
							case '9':
								rx = KEY_RIGHT;
								break;
								
							default:   
								rx = KEY_JUNK;
						}
					} 
					else if (ch == '2') 
					{
						ch = read_with_timeout(2);
						if(ch == NO_OP_IDLE || ch == NO_OP_COMMAND)
							return (KEY_JUNK);
						if(ch == KEY_RESIZE)
							goto done;
						ch &= 0x7f;
						
						if(ch == '1')
							rx = KEY_DOWN;
						else
							rx = KEY_JUNK;
					}
					else 
					{
						rx = KEY_JUNK;
					}
					swallow_till_z:

					while (ch != 'z' && ch != '~' )
					{
						/* Read characters in escape sequence. "z"
						 ends things for suns and "~" for vt100's
						*/
						ch = read_with_timeout(2);
						if (ch == NO_OP_IDLE || ch == NO_OP_COMMAND)
							return(KEY_JUNK);
						
						if (ch == KEY_RESIZE)
							goto done;
						
						ch &= 0x7f;
					}
					return(rx);

				default:
					/* DEC function keys */
					num_keys = 0;
					do
					{
						ch = read_with_timeout(2);
						if(ch == NO_OP_IDLE || ch == NO_OP_COMMAND)
							break;
						
						if(ch == KEY_RESIZE)
							goto done;
						
						ch &= 0x7f;
					} while (num_keys++ < 6 && ch != '~');
				/* return junk below */
			}
		} 
		else if(ch == '?')
		{
			/* DEC vt52 application keys, and some Zenith 19 */
			ch = read_with_timeout(time_out);
			if(ch == NO_OP_IDLE || ch == NO_OP_COMMAND ||
				ch == KEY_RESIZE) 
				goto done;
			
			ch &= 0x7f;
			
			switch(ch) 
			{
				case 'r': 
					return(KEY_DOWN);
					
				case 't':
					return(KEY_LEFT);
					
				case 'v':
					return(KEY_RIGHT);
					
				case 'x':
					return(KEY_UP);
				/* default: return junk below */
			}
		}
		else if(ch == '\033')
		{
			/* special hack to get around comm devices eating
			* control characters.
			*/
			ch = read_with_timeout(time_out);
			if(ch == NO_OP_IDLE || ch == NO_OP_COMMAND ||
				ch == KEY_RESIZE) 
				goto done;
			ch &= 0x7f;

#ifndef islower
#define islower(x)	((x) >= 'a' && (x) <= 'z')
#endif

#ifndef toupper
#define toupper(x)	((x) - ('a' - 'A'))
#endif

#ifndef isalpha
#define isalpha(x)	(((x) >= 'A' && (x) <= 'Z') || ((x) >= 'a' && (x) <= 'z'))
#endif
			if (islower(ch))		/* canonicalize if alpha */
			{
				ch = toupper(ch);
			}

			return ((isalpha(ch) || ch == '@' || (ch >= '[' && ch <= '_'))? ctrl(ch) : ch);
		} 
		else
		{
			/* This gets most Z19 codes, and some VT52 modes */
			/* NOTE: A == KEY_UP, B == KEY_DOWN... */
			return((ch < 'A' || ch > 'D') ? KEY_JUNK : (KEY_UP + (ch - 'A')));
		}

		/*
		* If we got here we didn't have anything useful to return, so
		* just let the caller know we got junk...
		*/
		return(KEY_JUNK);


		default:
		done:
			dprint(debugfile, 9,  "Read char returning: %c \n", ch);
			return(ch);
	}
}
#endif

#if 0
int main(void)
{
	int i;
	int c = 0;

	if (signal(SIGINT, sig_catcher) == SIG_ERR)
	{
		fprintf(stderr, "set SIGINT error!\n");
		return 1;

	}
	if (signal(SIGQUIT, sig_catcher) == SIG_ERR)
	{
		fprintf(stderr, "set SIGQUIT error!\n");
		return 1;

	}

	if (signal(SIGTERM, sig_catcher) == SIG_ERR)
	{
		fprintf(stderr, "set SIGTERM error!\n");
		return 1;

	}

	if (tty_raw(STDIN_FILENO) < 0)
	{
		fprintf(stderr, "enter raw mode error!\n");
		return 1;
	}

	printf("enter raw mode\n");

	debugfile = new_debug_file("debug.txt", 1);

/*
	while ((i = read(STDIN_FILENO, &c, 1)) == 1)
	{
		if ((c & 255) == 0177)
		{
			break;
		}

		printf("%o\n", c);

		if (c == 'q')
		{
			break;
		}
	}
*/
	while ((c = read_char(10)) != NO_OP_COMMAND)
	{
		switch (c)
		{
			case KEY_LEFT:
				printf("left key is pressed!\n");
				break;

			case KEY_RIGHT:
				printf("right key is pressed!\n");
				break;

			case KEY_UP:
				printf("up key is pressed!\n");
				break;

			case KEY_DOWN:
				printf("down key is pressed!\n");
				break;
				
			default:
				printf("0x%x, %c\n",c, c);
				break;
		}

		if (c == NO_OP_IDLE)
		{
			break;
		}
	}

	tty_reset(STDIN_FILENO);

	close_debug_file(debugfile);

	return 1;
}
#endif

