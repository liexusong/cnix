#include <cnix/stdarg.h>
#include <cnix/string.h>

#define SI	0
#define UI	1
#define OI	2
#define xI	3
#define XI	4
#define MAX	XI

int fillbuf(char * buf, int idx, char * tmp, int limit, char ch1)
{
	int len, idx1 = idx;

	len = 0;
	while(tmp[len])
		len++;

	if(len < limit){
		len = limit - len;
		while(len > 0){
			buf[idx1++] = ch1;
			len--;
		}
	}

	while(*tmp){
		buf[idx1++] = *tmp;
		tmp++;
		if(limit && (idx1 - idx) > limit)
			break;
	}

	return idx1 - idx;
}

static int number(char * buf, int idx, int num, int type, int limit, char ch1)
{
	int i, j, scale, flag;
	unsigned int nownum;
	int scales[XI + 1] = {10, 10, 8, 16, 16};
	char tmp[32], tmp1[32];
	char * model1 = "0123456789abcdef";
	char * model2 = "0123456789ABCDEF";
	char * digit;

	if(type > MAX)
	       return 0;

	flag = 0;
	if(num < 0 && type == SI){
		flag = 1;
		nownum = (unsigned int)(-num);
	}else
		nownum = (unsigned int)num;

	if(type == XI)
		digit = model2;
	else
		digit = model1;
	
	scale = scales[type];

	j = 0;
	if(nownum > 0){
		while(nownum > 0){
			tmp[j++] = digit[nownum % scale];
			nownum /= scale;
		}
	}else
		tmp[j++] = '0';

	i = 0;
	while(j > 0)
		tmp1[i++] = tmp[--j];

	tmp1[i] = '\0';

	if(flag)
		buf[idx++] = '-';

	return (fillbuf(buf, idx, tmp1, limit, ch1) + flag);
}

static int formatch(char * buf,
	int idx, char ch, va_list * ap, int limit, char ch1)
{
	char * tmp;
	int ret = 0;

	switch(ch){
	case 'd':
	case 'i':	
		ret = number(buf, idx, va_arg(*ap, int), SI, limit, ch1);
		break;
	case 'u':
		ret = number(buf,
			idx, va_arg(*ap, unsigned int), UI, limit, ch1);
		break;
	case 'o':
		ret = number(buf,
			idx, va_arg(*ap, unsigned int), OI, limit, ch1);
		break;
	case 'x':
		ret = number(buf,
			idx, va_arg(*ap, unsigned int), xI, limit, ch1);
		break;
	case 'X':
		ret = number(buf,
			idx, va_arg(*ap, unsigned int), XI, limit, ch1);
		break;
	case 'c': /* ignore limit */
		buf[idx] = va_arg(*ap, int);
		ret = 1;
		break;
	case 's':
		tmp = va_arg(*ap, char *);
		ret = fillbuf(buf, idx, tmp, limit, ch1);
		break;
	default:
		break;
	}

	return ret;
}

int vsprintf(char * buf, const char * fmt, va_list ap)
{
	char ch, ch1 = ' ', * fmtptr;
	int i, limit, len, state;

	state = 0;
	limit = 0;

	for(fmtptr = (char *)fmt, i = 0; *fmtptr; fmtptr++){
		ch = *fmtptr;

		switch(state){
		case 0:
			if(ch == '%'){
				state = 1;
				limit = 0;
				ch1 = ' ';
			}else
				buf[i++] = ch;
			break;
		case 1:
			switch(ch){
			case '0':
				if((ch1 == ' ') && !limit)
					ch1 = '0';
				else
					limit = limit * 10;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				limit = limit * 10 + ch - '0';
				break;
			default:
				len = formatch(buf, i, ch, &ap, limit, ch1);
				if(!len){
					buf[i++] = '%';
					buf[i++] = ch;
				}else
					i+= len;

				state = 0;
				break;
			}

			break;
		default: 	/* can't happen */
			break;
		}
	}

	buf[i] = '\0';

	return i;
}
