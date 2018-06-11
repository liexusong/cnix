#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "screen.h"
#include "utils.h"

/*
 * copy from manual of linux named console_codes.
 *
 CSI  (or  ESC  [) is followed by a sequence of parameters, at most NPAR
       (16), that are decimal numbers separated by  semicolons.  An  empty  or
       absent  parameter  is taken to be 0.  The sequence of parameters may be
       preceded by a single question mark.

       However, after CSI [ (or ESC [ [) a single character is read  and  this
       entire  sequence  is ignored. (The idea is to ignore an echoed function
       key.)

       The action of a CSI sequence is determined by its final character.


       @   ICH       Insert the indicated # of blank characters.
       A   CUU       Move cursor up the indicated # of rows.
       B   CUD       Move cursor down the indicated # of rows.
       C   CUF       Move cursor right the indicated # of columns.
       D   CUB       Move cursor left the indicated # of columns.
       E   CNL       Move cursor down the indicated # of rows, to column 1.
       F   CPL       Move cursor up the indicated # of rows, to column 1.
       G   CHA       Move cursor to indicated column in current row.
       H   CUP       Move cursor to the indicated row, column (origin at 1,1).
       J   ED        Erase display (default: from cursor to end of display).
                     ESC [ 1 J: erase from start to cursor.
                     ESC [ 2 J: erase whole display.
       K   EL        Erase line (default: from cursor to end of line).
                     ESC [ 1 K: erase from start of line to cursor.
                     ESC [ 2 K: erase whole line.
       L   IL        Insert the indicated # of blank lines.
       M   DL        Delete the indicated # of lines.
       P   DCH       Delete the indicated # of characters on the current line.
       X   ECH       Erase the indicated # of characters on the current line.
       a   HPR       Move cursor right the indicated # of columns.
       c   DA        Answer ESC [ ? 6 c: ‚ÄòI am a VT102‚Ä?
       d   VPA       Move cursor to the indicated row, current column.
       e   VPR       Move cursor down the indicated # of rows.
       f   HVP       Move cursor to the indicated row, column.
       g   TBC       Without parameter: clear tab stop at the current position.
                     ESC [ 3 g: delete all tab stops.
       h   SM        Set Mode (see below).
       l   RM        Reset Mode (see below).
       m   SGR       Set attributes (see below).
       n   DSR       Status report (see below).
       q   DECLL     Set keyboard LEDs.
                     ESC [ 0 q: clear all LEDs
                     ESC [ 1 q: set Scroll Lock LED
                     ESC [ 2 q: set Num Lock LED
                     ESC [ 3 q: set Caps Lock LED
      */

/*
 The ECMA‚Ä?8 SGR sequence ESC [ <parameters> m sets display  attributes.
       Several attributes can be set in the same sequence.


       par   result
       0     reset all attributes to their defaults
       1     set bold
       2     set half‚Äêbright (simulated with color on a color display)
       4     set underscore (simulated with color on a color display)
             (the colors used to simulate dim or underline are set
             using ESC ] ...)
       5     set blink
       7     set reverse video
       10    reset selected mapping, display control flag,
             and toggle meta flag.
       11    select null mapping, set display control flag,

             reset toggle meta flag.
       12    select null mapping, set display control flag,
             set toggle meta flag. (The toggle meta flag
             causes the high bit of a byte to be toggled
             before the mapping table translation is done.)
       21    set normal intensity (this is not compatible with ECMA‚Ä?8)
       22    set normal intensity
       24    underline off
       25    blink off
       27    reverse video off
       
       30    set black foreground
       31    set red foreground
       32    set green foreground
       33    set brown foreground
       34    set blue foreground
       35    set magenta foreground
       36    set cyan foreground
       37    set white foreground
       
       38    set underscore on, set default foreground color
       39    set underscore off, set default foreground color
       
       40    set black background
       41    set red background
       42    set green background
       43    set brown background
       44    set blue background
       45    set magenta background
       46    set cyan background
       47    set white background
       49    set default background color

      */


/// clear the whole screen
void screen_clear(void)
{
	printf("\033[2J");
}

/// switch the screen to normal state
void screen_normal(void)
{
	printf("\033[0m");
}

/// set the screen background color
void screen_set_backcolor(int color)
{
	char tmp[20];

	if (color < BACK_COLOR_BLACK || color > BACK_COLOR_WHITE)
	{
		return;
	}

	sprintf(tmp, "\033[%dm", color);

	printf("%s", tmp);
}

///
void screen_set_forecolor(int color)
{
	char tmp[20];

	if (color < FORE_COLOR_BLACK || color > FORE_COLOR_WHITE)
	{
		return;
	}

	sprintf(tmp, "\033[%dm", color);

	printf("%s", tmp);
}

/// delete n chars at the current line
void screen_delete_nchars(int n)
{
	char tmp[20];

	sprintf(tmp, "\033[%dX", n);
	printf("%s", tmp);
}

///  cursor move down
void screen_cursor_down(void)
{
	printf("\033[1B");
}

/// cursor move up
void screen_cursor_up(void)
{
	printf("\033[1A");
}

/// cursor move right
void screen_cursor_right(void)
{
	printf("\033[1C");
}

/// cursor move left
void screen_cursor_left(void)
{
	printf("\033[1D");
}

///
void screen_gotoxy(int x, int y)
{
	char tmp[20];

	if (x < 1 || y < 1)
	{
		return;
	}

	sprintf(tmp, "\033[%d;%dH", y, x);
	printf("%s", tmp);
}

/// draw the vertical or horizontal line
void screen_draw_line(int sx, int sy, int ex, int ey, int color)
{
	screen_set_backcolor(color);

	screen_gotoxy(sx, sy);
	
	// draw vertical line
	if (sx == ex)
	{
		while (1)
		{
			printf(" ");

			if (++sy > ey)
			{
				screen_cursor_left();
				break;
			}

			screen_cursor_down();
			screen_cursor_left();
		}
#if 0
		for (; sy <= ey; sy++)
		{
			printf(" ");
			
			screen_cursor_down();
			screen_cursor_left();
		}
#endif
	}
	else if (sy == ey)
	{
		for (; sx <= ex; sx++)
		{
			printf(" ");
		}

		screen_cursor_left();
	}
}


/// clean one line at (x, y)
void screen_clean_line(int x, int y, int width)
{
	screen_gotoxy(x, y);
	screen_delete_nchars(width);
}

/// clear the region of screen
void screen_clean_region(int x, int y, int w, int h)
{
	int i;

	screen_set_backcolor(BACK_COLOR_BLACK);
	for (i = 0; i < h; i++)
	{
		screen_clean_line(x, y + i, w);
	}
}

///
void screen_draw_region(int x, int y, int w, int h, int color)
{
	int i;

	for (i = 0; i < h; i++)
	{
		screen_draw_line(x, y + i, x + w - 1, y + i, color);
	}
}

/// draw border
void screen_draw_border(int x, int y, int w, int h, int color)
{
	screen_gotoxy(x, y);

	screen_draw_line(x, y, x + w - 1, y, color);
	screen_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);

	screen_gotoxy(x, y);
	screen_draw_line(x, y, x, y + h - 1, color);
	screen_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
}



#if 0
int main(void)
{
	screen_clear();

	tty_raw(STDIN_FILENO);

	screen_gotoxy(1, 1);

//	screen_draw_border(1, 1, 10, 10, BACK_COLOR_RED);
//	screen_draw_border(3, 3, 5, 5, BACK_COLOR_GREEN);


	screen_draw_region(5, 5, 2, 2, BACK_COLOR_BLUE);

	
	read_char(-1);

	screen_clean_region(5, 5, 2, 2);


	read_char(-1);
	
	screen_draw_region(6, 5, 2, 2, BACK_COLOR_RED);

	screen_gotoxy(1, 23);
	screen_draw_region(1, 25, 10, 1, BACK_COLOR_BLUE);

	screen_cursor_down();
	screen_cursor_down();
	
	screen_normal();

	fflush(stdout);

	tty_reset(STDIN_FILENO);

	return 0;	
}
#endif
