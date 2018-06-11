/*
 * snake.c
 * author forest077 form chinaunix, modified for cnix by xiexiecn.
 */
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

/* rand function code: from isdn */

#define RANDOM_MAX 0x7FFFFFFF

static long my_do_rand(unsigned long *value)

{
   long quotient, remainder, t;

   quotient = *value / 127773L;
   remainder = *value % 127773L;
   t = 16807L * remainder - 2836L * quotient;

   if (t <= 0)
      t += 0x7FFFFFFFL;

   return ((*value = t) % ((unsigned long)RANDOM_MAX + 1));
}

static unsigned long next = 1;

int rand(void)
{
   return my_do_rand(&next);
}

void srand(unsigned int seed)
{
   next = seed;
}

/* rand code end */

#define SNAKE_INITX 5
#define SNAKE_INITY 5
#define SNAKE_SHAPE '*'
#define SNAKE_INITLEN 8

#define WIN_X1 1
#define WIN_X2 80
#define WIN_Y1 1
#define WIN_Y2 24

//#define MAX_LEVEL 20
#define MAX_LEVEL 10
#define MAX_INTER 200000
#define MIN_INTER 0
#define MAX_RICH 10
#define DEVRATE 5
#define OVER "Game Over!!!"

struct stNode
{
	int x;
	int y;
	char shape;
	struct stNode *next;
};

struct stFood
{
	int x;
	int y;
};

struct stNode *gpstHead, *gpstTail;
struct stFood gastFood[MAX_RICH];
int giLevel = 1;
int giRich = 1;
int giScore = 0;
int giLen = 0;

#define MAX_NODE_NUM 1024

struct stNode nodeArray[MAX_NODE_NUM];
char bFreeForNode[MAX_NODE_NUM];

#if 0
void * malloc(int size);
void free(void * buff);
#else
#define malloc Malloc
#define free Free

void * Malloc(int size)
{
	int i;

	for(i = 0; i < MAX_NODE_NUM; i++)
		if(bFreeForNode[i]){
			bFreeForNode[i] = 0;
			return &nodeArray[i];
		}

	return NULL;
}

void Free(void * buff)
{
	int idx;

	idx = ((unsigned long)buff - (unsigned long)nodeArray)
		/ sizeof(struct stNode);

	bFreeForNode[idx] = 1;
}
#endif

struct termios stOldTerm;

void settty(int iFlag, int level)
{
	struct termios stTerm;

	if(iFlag == 1){
		tcgetattr(0, &stTerm);

		stOldTerm = stTerm;

		stTerm.c_lflag &= ~ICANON;
		stTerm.c_lflag &= ~ECHO;
		stTerm.c_cc[VTIME] = 10 - level;
		stTerm.c_cc[VMIN] = 0;
		stTerm.c_iflag &= ~ISTRIP;
		stTerm.c_cflag |= CS8;
		stTerm.c_cflag &= ~PARENB;

		tcsetattr(0, TCSANOW, &stTerm);
	}else
		tcsetattr(0, TCSANOW, &stOldTerm);
}

void vDrawOneNode(struct stNode *pstNode,int iFlag)
{
	printf("\033[%d;40;%dm\033[%d;%dH%c",
		iFlag, iFlag*3+30, pstNode->y, pstNode->x, pstNode->shape);
}

void vDrawOneFood(int x,int y)
{
	printf("\033[1;40;36m\033[%d;%dH%c", y, x, '@');
}

int iGetDir(int iOriDir)
{
	int iFlag = 0;
	unsigned short cCh = 0;

#if 0
	int iRet;
	fd_set rset;
	struct	timeval	hTmo;

	FD_ZERO(&rset);
	FD_SET(0,&rset);

	hTmo.tv_sec = 0;
	hTmo.tv_usec =
		MAX_INTER - (MAX_INTER - MIN_INTER) / MAX_LEVEL * giLevel;

	iRet = select(1, &rset, NULL, NULL, &hTmo);
	if(iRet <= 0)
		return(iOriDir);
#endif

	for(;;){
		read(0, &cCh, 1);

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
					return((!((cCh - 65)
						^ iOriDir ^ 1))^(cCh - 65));
			default:
				return(iOriDir);
			}
		}
	}

	// added by myself, avoiding warning
	return(iOriDir);
}

void vInitScreen()
{
	settty(1, giLevel);
	printf("\033[?25l\033[2J");
}

void vRestoreScreen()
{
	printf("\033[24;1H\033[1m\033[40;34m\033[?25h");
	settty(0, giLevel);
}

void vDrawScope()
{
	int i,j;
	
	for(j = WIN_Y1; j <= WIN_Y2; j += WIN_Y2 - WIN_Y1){
		printf("\033[%d;%dH+", j, WIN_X1);
		for(i = WIN_X1 + 1; i < WIN_X2; i++)
			printf("-");

		printf("+");
	}

	for(i = WIN_Y1 + 1; i < WIN_Y2; i++){
		char buff[128];

		sprintf(buff, "\033[%d;%dH", i, WIN_X1);
		strcat(buff, "|");
		for(j = strlen(buff); j < WIN_X2 - WIN_X1 - 1; j++)
			buff[j] = ' ';
		buff[j + 1] = '\0';
		strcat(buff, "|");
	}
}

void vCreateSnake()
{
	int i;
	struct stNode *pstNew;

	gpstHead = (struct stNode*)malloc(sizeof(struct stNode));
	gpstHead->x = SNAKE_INITX;
	gpstHead->y = SNAKE_INITY;
	gpstHead->shape = SNAKE_SHAPE;
	gpstHead->next = NULL;
	vDrawOneNode(gpstHead, 1);
	gpstTail = gpstHead;
	for(i = 1;i < SNAKE_INITLEN; i++){
		pstNew = (struct stNode*)malloc(sizeof(struct stNode));
		pstNew->x = gpstHead->x + 1;	
		pstNew->y = gpstHead->y;
		pstNew->shape = SNAKE_SHAPE;
		pstNew->next = NULL;

		vDrawOneNode(pstNew,1);

		gpstHead->next = pstNew;
		gpstHead = pstNew;
	}

	return;
}

void vKillSnake()
{
	struct stNode *pstNode;

	for(pstNode = gpstTail; gpstTail != NULL;){
		gpstTail = pstNode->next;
		free(pstNode);
		pstNode = gpstTail;
	}
}

void vGenFood(int iIdx)
{
	struct stNode *pstNode;
	int i, iFound=0;

	for(; !iFound;){
		iFound = 1;

		gastFood[iIdx].x = rand() % (WIN_X2 - WIN_X1 - 1) + WIN_X1 + 1;
		gastFood[iIdx].y = rand() % (WIN_Y2 - WIN_Y1 - 1) + WIN_Y1 + 1;

		for(i = 0; i < giRich; i++){
			if(i != iIdx && gastFood[iIdx].x == gastFood[i].x
				&& gastFood[iIdx].y == gastFood[i].y){
				iFound = 0;
				break;
			}
		}

		if(!iFound)
			continue;

		for(pstNode = gpstTail;
			pstNode != NULL; pstNode = pstNode->next){
			if(gastFood[iIdx].x == pstNode->x
				&& gastFood[iIdx].y == pstNode->y){
				iFound = 0;
				break;
			}
		}

		if(!iFound)
			continue;
	}

	vDrawOneFood(gastFood[iIdx].x, gastFood[iIdx].y);
}

void vInitFood()
{
	int i;
	
	srand(790630);
	for(i = 0; i< giRich; i++)
		vGenFood(i);
}
	
int iIsValid(int x,int y)
{
	struct stNode *pstNode;

	if(x <= WIN_X1 || x >= WIN_X2 || y <= WIN_Y1 || y >= WIN_Y2)
		return(0);

	pstNode = gpstTail;
	for(; pstNode != NULL;){
		if(x == pstNode->x && y == pstNode->y)
			return(0);

		pstNode = pstNode->next;
	}

	return(1);
}

int iEat(int x,int y)
{
	int i;
	
	for(i = 0;i < giRich; i++){
		if(x == gastFood[i].x && y == gastFood[i].y){
			vGenFood(i);
			giScore += giLevel*10;
			giLen++;

			if(giLevel < MAX_LEVEL)
				if(giLen % DEVRATE==0){
					giLevel++;
					settty(1, giLevel);
				}
			return(1);
		}
	}

	return(0);
}

int main(void)
{
	int i, iDir = 2, iNextX, iNextY;
	struct stNode *pstNew;
	char sPrompt[80];

	for(i = 0; i < MAX_NODE_NUM; i++)
		bFreeForNode[i] = 1;
	
	vInitScreen();
	vDrawScope();
	vCreateSnake();
	vInitFood();

	fflush(stdout);

	for(;;){
		iDir = iGetDir(iDir);
		iNextX = gpstHead->x + (iDir >> 1) * (5 - (iDir << 1));
		iNextY = gpstHead->y - (!(iDir >> 1))*(1 - (iDir << 1));

		if(!iIsValid(iNextX,iNextY)){
			printf("\033[%d;%dH\033[1m\033[40;34m%s\033[0m",
				WIN_Y2 - 1,
				(int)((WIN_X1 + WIN_X2) / 2 - strlen(OVER) / 2),
				OVER
				);
			break;
		}

		pstNew = (struct stNode*)malloc(sizeof(struct stNode));
		pstNew->x = iNextX;
		pstNew->y = iNextY;
		pstNew->shape = SNAKE_SHAPE;
		pstNew->next = NULL;
		gpstHead->next = pstNew;
		gpstHead = pstNew;

		vDrawOneNode(gpstHead,1);
		if(!iEat(iNextX,iNextY)){
			vDrawOneNode(gpstHead, 1);
			vDrawOneNode(gpstTail, 0);
			pstNew = gpstTail;
			gpstTail = pstNew->next;
			free(pstNew);
		}

		sprintf(sPrompt, "Score:%7d Level:%2d", giScore, giLevel);
		printf("\033[%d;%dH\033[1m\033[40m\033[34m%s\033[0m",
			WIN_Y2,
			(int)((WIN_X1 + WIN_X2) / 2 - strlen(sPrompt) / 2),
			sPrompt
			);

		fflush(stdout);
	}

	vKillSnake();

	vRestoreScreen();

	return 0;
}
