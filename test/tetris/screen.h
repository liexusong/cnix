#ifndef SCREEN_H
#define SCREEN_H

enum  
{
	FORE_COLOR_BLACK	= 30,
	FORE_COLOR_RED		= 31,
	FORE_COLOR_GREEN	= 32,
	FORE_COLOR_BROWN	= 33,
	FORE_COLOR_BLUE		= 34,
	FORE_COLOR_MAGENTA	= 35,
	FORE_COLOR_CYAN		= 36,
	FORE_COLOR_WHITE	= 37,
	
	BACK_COLOR_BLACK	= 40,
	BACK_COLOR_RED		= 41,
	BACK_COLOR_GREEN	= 42,
	BACK_COLOR_BROWN	= 43,
	BACK_COLOR_BLUE		= 44,
	BACK_COLOR_MAGENTA	= 45,
	BACK_COLOR_CYAN		= 46,
	BACK_COLOR_WHITE	= 47
};

///
void screen_draw_border(int x, int y, int w, int h, int color);

///
void screen_clear(void);

//
void screen_normal(void);

///
void screen_set_backcolor(int color);

///
void screen_draw_line(int sx, int sy, int ex, int ey, int color);

///
void screen_clean_line(int x, int y, int width);

///
void screen_clean_region(int x, int y, int w, int h);

///
void screen_draw_region(int x, int y, int w, int h, int color);

///
void screen_cursor_down(void);

///
void screen_set_forecolor(int color);

void screen_gotoxy(int x, int y);

#endif
