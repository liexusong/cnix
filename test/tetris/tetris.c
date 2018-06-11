#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#ifndef CNIX
#include <sys/time.h>
#endif

#include "screen.h"
#include "input.h"
#include "utils.h"

/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */
#define cmpxchg(ptr,o,n)\
	((__typeof__(*(ptr)))__cmpxchg((ptr),(unsigned long)(o),\
					(unsigned long)(n),sizeof(*(ptr))))

static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
				      unsigned long new, int size)
{
	unsigned long prev;
	volatile char *ptr1 = (char *)ptr;
	volatile unsigned short *ptr2 = (unsigned short *)ptr;
	volatile unsigned long *ptr3 = (unsigned long *)ptr;
	
	switch (size)
	{
		case 1:
		{
			__asm__ __volatile__("cmpxchgb %b1,%2"
					     : "=a"(prev)
					     : "q"(new), "m"(*(ptr1)), "0"(old)
					     : "memory");
		
			return prev;
		}
		
		case 2:
			__asm__ __volatile__("cmpxchgw %w1,%2"
					     : "=a"(prev)
					     : "r"(new), "m"(*(ptr2)), "0"(old)
					     : "memory");
			return prev;
		case 4:
			__asm__ __volatile__("cmpxchgl %1,%2"
					     : "=a"(prev)
					     : "r"(new), "m"(*(ptr3)), "0"(old)
					     : "memory");
			return prev;
	}
	return old;
}


/// added by qxx 2008/4/18
typedef struct point
{
	int x;
	int y;
} point_t;


///
typedef struct _box_t
{
	point_t box_orgin;		// the postion of origin
	int	box_shape;		// the box shape
	int	box_rotate;		// the box currrent rotate index.
} box_t;

typedef struct _rect_t
{
	int x;
	int y;
	int w;
	int h;
} rect_t;

#define min(x, y)	((x) > (y) ? (y) : (x))
#define max(x, y)	((x) < (y) ? (y) : (x))

typedef enum _dir_t
{
	DIR_LEFT,		// 左移动整个方块
	DIR_RIGHT		// 右移动整个方块
} dir_t;

#define BOX_NUMS	4
#define SHAPE_NUMS	7

#define MAX_LEVEL	3


#define BOARD_HEIGHT	20
#define BOARD_WIDTH	10

#define BOX_EMPTY	0

#define ORGIN_X		1
#define ORGIN_Y		1
#define BOX_DIMENSION	1

/// the 0th elments will not be used.
/// 1th & REAL - 1 will be used as border
static int game_board[BOARD_HEIGHT + 1 + 2][BOARD_WIDTH + 1 + 2];

///
static box_t cur_box;			// current moving box

///
static box_t next_box; 		// the next box

///
static int game_score;	

///
static int game_level = 1;

///
static int box_color_type[7];

debug_file_t *debugfile;

static BOOLEAN	in_handler = 0;

static BOOLEAN is_game_over;

///
static int tetris_shapes[][4][4] =
{
	{
		{1, 1, 0, 0}, 
		{1, 1, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0}
	},
	
	{
		{1, 1, 0, 0}, 
		{1, 1, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0}
	},

	{
		{1, 1, 0, 0}, 
		{1, 1, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0}
	},
	{
		{1, 1, 0, 0}, 
		{1, 1, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0}
	},
	
	{
		{2, 2, 2, 0},
		{0, 0, 2, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},
	{
		{0, 2, 0, 0},
		{0, 2, 0, 0},
		{2, 2, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{2, 0, 0, 0},
		{2, 2, 2, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{2, 2, 0, 0},
		{2, 0, 0, 0},
		{2, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{3, 3, 3, 0},
		{3, 0, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{3, 3, 0, 0},
		{0, 3, 0, 0},
		{0, 3, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{0, 0, 3, 0},
		{3, 3, 3, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{3, 0, 0, 0},
		{3, 0, 0, 0},
		{3, 3, 0, 0},
		{0, 0, 0, 0},
	},


	{
		{4, 4, 0, 0},
		{0, 4, 4, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{0, 4, 0, 0},
		{4, 4, 0, 0},
		{4, 0, 0, 0},
		{0, 0, 0, 0},
	},
	
	{
		{4, 4, 0, 0},
		{0, 4, 4, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{0, 4, 0, 0},
		{4, 4, 0, 0},
		{4, 0, 0, 0},
		{0, 0, 0, 0},
	},
	{
		{0, 5, 5, 0},
		{5, 5, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{5, 0, 0, 0},
		{5, 5, 0, 0},
		{0, 5, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{0, 5, 5, 0},
		{5, 5, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{5, 0, 0, 0},
		{5, 5, 0, 0},
		{0, 5, 0, 0},
		{0, 0, 0, 0},
	},

 	{
		{0, 6, 0, 0},
		{6, 6, 6, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{6, 0, 0, 0,},
		{6, 6, 0, 0,},
		{6, 0, 0, 0,},
		{0, 0, 0, 0,},
	},

	{
		{6, 6, 6, 0},
		{0, 6, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{0, 6, 0, 0},
		{6, 6, 0, 0},
		{0, 6, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{7, 7, 7, 7},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{7, 0, 0, 0},
		{7, 0, 0, 0},
		{7, 0, 0, 0},
		{7, 0, 0, 0},
	},

	{
		{7, 7, 7, 7},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
	},

	{
		{7, 0, 0, 0},
		{7, 0, 0, 0},
		{7, 0, 0, 0},
		{7, 0, 0, 0},
	}
};

///
static int dimensions[][2] =
{
	{2, 2},
	{3, 2},
	{3, 2},
	{3, 2},
	{3, 2},
	{3, 2},
	{4, 1},
};

#ifndef CNIX
///
static long velocit_table[MAX_LEVEL] = {250000L, 150000L, 100000L};
#else
//static long velocit_table[MAX_LEVEL] = {90000L, 80000L, 60000L};
#endif

///
static BOOLEAN check_round_ok(box_t *box);

///
static BOOLEAN rotate_box(box_t *box);

///
static BOOLEAN move_box(box_t *box, dir_t d);

///
static void empty_board(void);

///
static BOOLEAN drop_down(box_t *box, int step);

///
static void OnKeyDown(box_t *box, int key_code);

///
static void OnTimeout(box_t *box);

//
static BOOLEAN get_next_box(box_t *pbox);

///
static void draw_box(box_t *box);

///
static void OnPaint(rect_t *erase_r, rect_t *paint_r);

///
static void start_timer(void);

///
static void stop_timer(void);

///
static void fill_box(box_t *box);

///
static void game_over(void);


///
static void generate_next_box(box_t *box);

///
static void draw_next_box(void);

///
static void get_box_region(const box_t *box, rect_t *rect);

//
static void enter_gamebox(void)
{
	rect_t erase_r;
	rect_t paint_r;
	
	empty_board();
	
	//
	box_color_type[0] 	= BACK_COLOR_RED;
	box_color_type[1] 	= BACK_COLOR_GREEN;
	box_color_type[2]	= BACK_COLOR_BROWN;
	box_color_type[3]	= BACK_COLOR_BLUE;
	box_color_type[4]	= BACK_COLOR_MAGENTA;
	box_color_type[5]	= BACK_COLOR_CYAN;
	box_color_type[6]	= BACK_COLOR_WHITE;
	
	game_score = 0;

	generate_next_box(&next_box);
	get_next_box(&cur_box);
//	get_next_box(&next_box);

	memset(&erase_r, 0, sizeof(erase_r));
	paint_r.x = 1;
	paint_r.y = 1;
	paint_r.w = BOARD_WIDTH + 2;
	paint_r.h = BOARD_HEIGHT + 2;
	
	OnPaint(&erase_r, &paint_r);
	draw_box(&cur_box);
	draw_next_box();

	start_timer();
}


///
int get_box_pos_x(const box_t *box)
{
	return box->box_orgin.x;
}

///
int get_box_pos_y(const box_t *box)
{
	return box->box_orgin.y;
}

///
int get_box_width(const box_t *box)
{
	return dimensions[box->box_shape][(box->box_rotate) & 1];
}

///
int get_box_height(const box_t *box)
{
	return dimensions[box->box_shape][(box->box_rotate + 1) & 1];
}

///
void set_box_position(box_t *box, int x, int y)
{
	box->box_orgin.x = x;
	box->box_orgin.y = y;
}

///
int get_box_value(const box_t *box, int x, int y)
{
	return tetris_shapes[box->box_shape * 4 + box->box_rotate][y][x];
}

// check the postion is ok?
static BOOLEAN check_round_ok(box_t *box)
{
	int i;
	int j;
	int pos_x;
	int pos_y;
	int box_width;
	int box_height;
	BOOLEAN ok;

	pos_x = get_box_pos_x(box);
	pos_y = get_box_pos_y(box);

	box_width = get_box_width(box);
	box_height = get_box_height(box);

	ok = ((pos_x >= 2) && ((pos_x + box_width) <= (BOARD_WIDTH + 2)));
	ok = ok && ((pos_y >= 2) && ((pos_y + box_height) <= (BOARD_HEIGHT + 2)));

	if (!ok)
	{
		return ok;
	}

	for (j = 0; j < box_height; j++)
	{
		for (i = 0; i < box_width; i++)
		{
			if (!get_box_value(box, i, j))
			{
				continue;
			}
			
			if (game_board[pos_y + j][pos_x + i])
			{
				return FALSE;
			}
		}
	}

	return TRUE;	
}

/// rotate the box
static BOOLEAN rotate_box(box_t *box)
{
	BOOLEAN ok = TRUE;
	int rotate;

	rotate = box->box_rotate;

	box->box_rotate++;
	box->box_rotate &= 3;

	if (!check_round_ok(box))
	{
		ok = FALSE;
		box->box_rotate = rotate;
	}
	
	return ok;
}


/// move the box
static BOOLEAN move_box(box_t *box, dir_t d)
{
	int x = box->box_orgin.x;
	BOOLEAN ok = TRUE;

	if (d == DIR_LEFT)
	{
		box->box_orgin.x--;
	}
	else if (d == DIR_RIGHT)
	{
		box->box_orgin.x++;
	}

	ok = check_round_ok(box);
	if (!ok)
	{
		box->box_orgin.x = x;
	}

	return ok;
}


/*
 * drop steps 
 * TRUE ==> the box dosn't stop
 * FALSE ==> the box has reached the gameboard.
 */
static BOOLEAN drop_down(box_t *box, int step)
{
	int y;
	int orig_y;

	orig_y = box->box_orgin.y;

	if (step < 0)
	{
		step = 0xFFFF;		// max value
	}

	while (step)
	{
		y = box->box_orgin.y;
		box->box_orgin.y++;
		step--;

		if (!check_round_ok(box))
		{
			box->box_orgin.y = y;

			fill_box(box);

			return FALSE;
		}
	}
	
	return TRUE;
}

///
static void fill_box(box_t *box)
{
	int i;
	int j;
	int w;
	int h;
	int pos_x;
	int pos_y;

	w = get_box_width(box);
	h = get_box_height(box);

	pos_x = get_box_pos_x(box);
	pos_y = get_box_pos_y(box);

	for (j = 0; j < h; j++)
	{
		for (i = 0; i < w; i++)
		{
			if (!get_box_value(box, i, j))
			{
				continue;
			}
			
			game_board[j + pos_y][i + pos_x] = box->box_shape + 1;
		}
	}
}

///
static void empty_board(void)
{
	int x, y;

	for (y = 1; y <= BOARD_HEIGHT + 2; y++)
	{
		game_board[y][1] = 7;
		game_board[y][BOARD_WIDTH + 2] = 7;
	}
	
	for (x = 1; x <= BOARD_WIDTH + 2; x++)
	{
		game_board[1][x] = 7;
		game_board[BOARD_HEIGHT + 2][x] = 7;
	}
	

	for (y = 2; y < BOARD_HEIGHT + 2; y++)
	{
		for (x = 2; x < BOARD_WIDTH + 2; x++)
		{
			game_board[y][x] = BOX_EMPTY;
		}
	}
	
}

///
static BOOLEAN remove_lines(rect_t *rect)
{
	int i;
	int k;
	int board_index;
	int num = 0;
	int line_index = 0;
	int max_y;
	int min_y;
	BOOLEAN ok = TRUE;

	min_y = 2;
	while (1)
	{	// get the max blank line number
		ok = TRUE;
		for (i = 2; i < BOARD_WIDTH + 2; i++)
		{
			if (game_board[min_y][i] != BOX_EMPTY)
			{
				ok = FALSE;
				break;
			}
		}

		if (ok)
		{
			min_y++;
		}
		else
		{
			break;
		}
	}
	
	max_y = -1;
	for (board_index = BOARD_HEIGHT + 1; board_index >= min_y;)
	{
		num = 0;
		for (i = 2; i < BOARD_WIDTH + 2; i++)
		{
			if ((game_board[board_index][i]) != 0)
			{
				num++;
			}
		}
		
		if (num != BOARD_WIDTH)
		{
			board_index--;
			continue;
		}

		if (max_y < board_index)
		{
			max_y = board_index;
		}


		game_score += 10;
		line_index++;
		
		///
		for (k = board_index; k > min_y; k--)	// remove on line
		{
			for (i = 2; i < BOARD_WIDTH + 2; i++)
			{	
				game_board[k][i] = game_board[k - 1][i];
			}
		}

		// adjust the last line
		for (i = 2; i < BOARD_WIDTH + 2; i++)
		{
			game_board[min_y][i] = BOX_EMPTY;	
		}

		if (board_index == 2)
		{
			break;
		}
	}

	// some lines hasn't remove
	if (line_index == 0)
	{
		rect->x = 0;
		rect->y = 0;
	}
	else
	{
		rect->x = 2;
		rect->w = BOARD_WIDTH;

		rect->y = min_y;
		rect->h = max_y - min_y + 1;
	}
	

	return (line_index || (num == 0));
}

///
static int gen_new_shape(void)
{
	static unsigned int old_shape = 0;
	int ret = old_shape;

	old_shape = (unsigned int)my_rand();

	old_shape %= SHAPE_NUMS;
		
	return ret;
}

static void generate_next_box(box_t *box)
{
	int w, h;
	
	box->box_shape = gen_new_shape();
	box->box_rotate = 0;

	w = dimensions[box->box_shape][0];
	h = dimensions[box->box_shape][1];

	set_box_position(box, BOARD_WIDTH + 3 + 2, 2);	
}

///
static void draw_next_box(void)
{
	rect_t rect;

	get_box_region(&next_box, &rect);

	screen_clean_region(rect.x, rect.y, 4, 4);
	draw_box(&next_box);
}

/*
 * TRUE => OK
 * FALSE => Game over
 */
static BOOLEAN get_next_box(box_t *box)
{
	int w, h;

	box->box_orgin = next_box.box_orgin;
	box->box_shape = next_box.box_shape;
	box->box_rotate = 0;

	generate_next_box(&next_box);
	

	w = dimensions[box->box_shape][0];
	h = dimensions[box->box_shape][1];

	set_box_position(box, 
			2 + (BOARD_WIDTH / 2  -  w / 2) * BOX_DIMENSION,
			2);

	return check_round_ok(box);
}


///
static void draw_region(int x, int y, int color)
{
	screen_draw_region(x, y, BOX_DIMENSION, BOX_DIMENSION, color);
}

///
static void draw_score(int x, int y, int val, int color)
{
	static int old_score = -1;
	char tmp_str[32];

	if (old_score == val)
	{
		return;
	}

	old_score = val;

	sprintf(tmp_str, "Score %d", val);

	screen_gotoxy(x, y);
	screen_set_backcolor(BACK_COLOR_BLACK);
	screen_set_forecolor(color);
	printf("%s", tmp_str);
}

///
static void draw_level(int x, int y, int val, int color)
{
	static int old_level = -1;
	char tmp_str[32];

	if (old_level == val)
	{
		return;
	}
	
	old_level = val;
	
	sprintf(tmp_str, "Level %d", val);

	screen_gotoxy(x, y);
	screen_set_backcolor(BACK_COLOR_BLACK);
	screen_set_forecolor(color);
	printf("%s", tmp_str);
}


///
static void game_over(void)
{
	char *msg = "Game Over";
	int len;
	int x;
	int y;
	
	
	dprint(debugfile, 2, "game over!\n");
	
	stop_timer();

	is_game_over = TRUE;

	len = strlen(msg);

	x = 2 + (BOARD_WIDTH - len) / 2;
	y = (BOARD_HEIGHT + 3) / 2;

	screen_gotoxy(x, y);
	screen_set_backcolor(BACK_COLOR_BLUE);
	screen_set_forecolor(FORE_COLOR_RED);
	printf("%s", msg);
}

///
static void start_timer(void)
{
#ifdef CNIX
	alarm(1);
#else
	struct itimerval val;
	struct itimerval oval;

	val.it_interval.tv_sec = 0;
	val.it_interval.tv_usec = velocit_table[game_level - 1];
	
	val.it_value.tv_sec = 0;
	val.it_value.tv_usec = velocit_table[game_level - 1];

	if (setitimer(ITIMER_REAL, &val, &oval) < 0)
	{
		dprint(debugfile, 1, "error1 msg = %s\n", strerror(errno));
		return;
	}

#endif
}

///
static void stop_timer(void)
{

#ifdef CNIX
	alarm(0);
#else
	struct itimerval val;
	struct itimerval oval;

	val.it_interval.tv_sec = 0;
	val.it_interval.tv_usec = 0;		// 0.1s
	
	val.it_value.tv_sec = 0;
	val.it_value.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &val, &oval) < 0)
	{
		dprint(debugfile, 1, "error2 msg = %s\n", strerror(errno));
		return;
	}

#endif
}


///
static void sig_alarm_handler(int x)
{
	OnTimeout(&cur_box);
//	printf("Ok\n");
}


///
static void OnPaint(rect_t *erase_r, rect_t *paint_r) 
{
	int i;
	int j;
	int type; 

#if 0
	screen_clean_region(ORGIN_X, 
				ORGIN_Y, 
				(BOARD_WIDTH + 2) * BOX_DIMENSION, 
				(BOARD_HEIGHT + 2) * BOX_DIMENSION);

	for (i = ORGIN_Y; i < BOARD_HEIGHT + 3; i++)
	{
		for(j = ORGIN_X; j < BOARD_WIDTH + 3; j++)
		{
			type = game_board[i][j];
			if (type == BOX_EMPTY)
			{
				continue;
			}

			draw_region(j, i, box_color_type[type - 1]);	
		}
	}
#endif

	//dump_region(erase_r);
	//dump_region(paint_r);
	
	if (erase_r->x > 0 && erase_r->y > 0)
	{
		for (i = 0; i < erase_r->h; i++)
		{
			for(j = 0; j < erase_r->w; j++)
			{
				type = game_board[erase_r->y + i][erase_r->x + j];
				
				if (type == BOX_EMPTY)
				{
					screen_clean_region(erase_r->x + j, erase_r->y + i, 1, 1);
				}
			}
		}
	}

	for (i = 0; i < paint_r->h; i++)
	{
		for(j = 0; j < paint_r->w; j++)
		{
			type = game_board[paint_r->y + i][paint_r->x + j];
			
			if (type == BOX_EMPTY)
			{
				screen_clean_region(paint_r->x + j, paint_r->y + i, 1, 1);
				continue;
			}

			draw_region(paint_r->x + j, paint_r->y + i, box_color_type[type - 1]);	
		}
	}
	
	draw_score(BOARD_WIDTH + 3 + 2, BOARD_HEIGHT / 2, game_score, FORE_COLOR_RED);
	draw_level(BOARD_WIDTH + 3 + 2, BOARD_HEIGHT / 2 + 1, game_level, FORE_COLOR_RED);

	fflush(stdout);
}

static void get_box_region(const box_t *box, rect_t *rect)
{
	rect->x = get_box_pos_x(box);
	rect->y = get_box_pos_y(box);
	rect->w = get_box_width(box);
	rect->h = get_box_height(box);
}

#if 0
static void dump_region(const rect_t *r)
{
	dprint(debugfile, 1, "x = %d, y = %d, w = %d, h = %d\n", r->x, r->y, r->w, r->h);
}
#endif

#define is_rect_valid(r)	((r->x > 0) && (r->y > 0))


///
static BOOLEAN union_rect_region(rect_t *r1, const rect_t *r2)
{
	if (!is_rect_valid(r2)) 
	{
		return is_rect_valid(r1);
	}

	if (!is_rect_valid(r1))
	{
		r1->x = r2->x;
		r1->y = r2->y;
		r1->w = r2->w;
		r1->h = r2->h;

		return TRUE;
	}

	// r1 && r2 all are valid
	r1->x = min(r1->x, r2->x);
	r1->y = min(r1->y, r2->y);
	r1->w = max(r1->w, r2->w);
	r1->h = max(r1->h, r2->h);

	return TRUE;
}


///
static void OnKeyDown(box_t *box, int key_code) 
{
	rect_t erase_r;
	rect_t paint_r;
	rect_t paint_r2;
	BOOLEAN is_down = FALSE;

	if (cmpxchg(&in_handler, 0, 1))
	{
		return;
	}

//	stop_timer();

	dprint(debugfile, 5, "in_handler = %d\n", in_handler);
	dprint(debugfile, 2, "OnKeyDown1()\n");

	memset(&erase_r, 0, sizeof(erase_r));
	memset(&paint_r2, 0, sizeof(paint_r2));
	memset(&paint_r, 0, sizeof(paint_r));
	
	get_box_region(box, &erase_r);
	
	switch (key_code)
	{
		case 'h':
		case 'H':
		case KEY_LEFT:
			move_box(box, DIR_LEFT);
			get_box_region(box, &paint_r);
			
			break;
			
		case 'l':
		case 'L':
		case KEY_RIGHT:
			move_box(box, DIR_RIGHT);
			get_box_region(box, &paint_r);	
			
			break;
			
		case 'j':
		case 'J':
		case KEY_DOWN:
			stop_timer();

			if (!drop_down(box, 3)) // drop down 3 lines
			{
				remove_lines(&paint_r2);
				is_down = TRUE;
			}
	
			get_box_region(box, &paint_r);
			
			union_rect_region(&paint_r, &paint_r2);
			
			if (is_down && !get_next_box(box))
			{
				game_over();
				in_handler = FALSE;
				return;
			}

			start_timer();

			break;
			
		case 'k':
		case 'K':
		case KEY_UP:
			rotate_box(box);
			get_box_region(box, &paint_r);
			break;

		case '+':
			game_level++;

			if (game_level > MAX_LEVEL)
			{
				game_level = MAX_LEVEL;
			}

			stop_timer();
			
			start_timer();
		
			break;

		case '-':
			game_level--;

			if (game_level < 1)
			{
				game_level = 1;
			}
			
			stop_timer();
			
			start_timer();
		
			break;
		default:
			goto done;
	}

	OnPaint(&erase_r, &paint_r);
	draw_box(box);

	if (is_down)
	{
		draw_next_box();
	}

	dprint(debugfile, 2, "OnKeyDown2()\n");
done:
	in_handler = FALSE;

//	start_timer();
}


///
static void OnTimeout(box_t *box) 
{
	rect_t erase_r;
	rect_t paint_r2;
	rect_t paint_r;
	BOOLEAN is_down = FALSE;

	if (cmpxchg(&in_handler, 0, 1))
	{
#ifdef CNIX
		start_timer();
#endif
		return;
	}
	
	dprint(debugfile, 2, "OnTimeout1()\n");

	memset(&erase_r, 0, sizeof(erase_r));
	memset(&paint_r2, 0, sizeof(paint_r2));
	memset(&paint_r, 0, sizeof(paint_r));
	
	get_box_region(box, &erase_r);
	// can drop one row?
	if (!drop_down(box, 1))
	{
		remove_lines(&paint_r2);
		is_down = TRUE;
	}

	get_box_region(box, &paint_r);
	if (union_rect_region(&paint_r, &paint_r2))
	{
		dprint(debugfile, 3, "union rect ok!\n");
		OnPaint(&erase_r, &paint_r);
	}

	if (is_down && !get_next_box(box))
	{
		game_over();
	}

	draw_box(box);
	if (is_down)
	{
		draw_next_box();
	}

#ifdef CNIX
	start_timer();
#endif

	dprint(debugfile, 2, "OnTimeout2()\n");

	in_handler = FALSE;
}

///
static void draw_box(box_t *box)
{
	int i;
	int j;
	int w;
	int h;
	int pos_y;
	int pos_x;

	pos_x = get_box_pos_x(box);
	pos_y = get_box_pos_y(box);
	
	w = get_box_width(box);
	h = get_box_height(box);

	// clear the old box region
	for (j = 0; j < h; j++)
	{
		for (i = 0; i < w; i++)
		{
			if (get_box_value(box, i, j))
			{
				draw_region(pos_x + i, 
					pos_y + j,
					box_color_type[box->box_shape]);
			}
		}
	}

	fflush(stdout);
}


#if 1

int main(void)
{
	int c = 0;
	int i;
	int keys[] = {'h','H','j', 'J', 'k', 
		'K', 'l', 'L', '+', '-',
		KEY_UP, KEY_DOWN, 
		KEY_RIGHT, KEY_LEFT};
	
#if 1
	debugfile = new_debug_file("debug.txt", 0);

	my_srand(8831133UL);

	if (signal(SIGINT, sig_catcher) == SIG_ERR)
	{
#ifndef CNIX
		fprintf(stderr, "set SIGINT error!\n");
#endif		
		dprint(debugfile, 1, "%s", "set SIGINT error!\n");
		return 1;

	}
	
	if (signal(SIGQUIT, sig_catcher) == SIG_ERR)
	{
#ifndef CNIX	
		fprintf(stderr, "set SIGQUIT error!\n");
#endif
		dprint(debugfile, 1, "%s", "set SIGQUIT error!\n");

		return 1;

	}

	if (signal(SIGTERM, sig_catcher) == SIG_ERR)
	{
#ifndef CNIX	
		fprintf(stderr, "set SIGTERM error!\n");
#endif
		dprint(debugfile, 1, "%s", "set SIGTERM error!\n");

		return 1;

	}

	if (signal(SIGALRM, sig_alarm_handler) == SIG_ERR)
	{
#ifndef CNIX	
		fprintf(stderr, "set SIGALRM error!\n");
#endif
		dprint(debugfile, 1, "%s", "set SIGALRM error!\n");

		return 1;
	}

	
#endif	
	screen_clear();
	
	enter_gamebox();

	fflush(stdout);

	if (tty_raw(STDIN_FILENO) < 0)
	{
#ifndef CNIX	
		fprintf(stderr, "enter raw mode error!\n");
#endif
		return 1;
	}

	do
	{
		if (is_game_over)
		{
			break;
		}
		
		c = read_char(-1);

		if (c == 'q' || c == 'Q')
		{
			break;
		}

		for (i = 0; i < sizeof(keys) / sizeof(int); i++)
		{
			if (c == keys[i])
			{
				break;
			}
		}

		if (i == sizeof(keys) / sizeof(int))
		{
			continue;
		}

		OnKeyDown(&cur_box, c);
	}  while(1);	

	screen_cursor_down();

	tty_reset(STDIN_FILENO);
	screen_normal();

	fflush(stdout);

#if 1
	close_debug_file(debugfile);
#endif

	return 0;	
}

#endif

/* 
 * read_with_timeout => read ... wait
 			     alarm  occurs
 			| PARAM1	|
 			| RET		|
 			| EBP		|
			| SS		|
			| ESP		|
			| EFLAGS	|
			| CS 		|	
 			| EIP		|
 			| INDEX		|
 			| ES		|
 			| DS		|

2\
			| PARAM1	|
 			| RET		|
 			| EBP		|

/////
sig_handler();
sig_return();
*/ 			     
