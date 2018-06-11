#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>

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
	0xffffffff
};

char arg_buff[1024], * arg_ptr[32];
int arg_num, arg_index;

char * mystrtok(char *, char *);
void do_cat(char *);
void do_reboot(void);
void do_execve(char *, char *, char *, char *);

int main(int argc, char * argv[])
{
	int len;
	char buff[1024];
	char * arg0, * arg1, * arg2, * arg3;
	
	signal(SIGINT, SIG_IGN);

	for(;;){
		printf("[xiexiecn@cnix/]#");

		fflush(stdout);

		len = read(0, buff, 1023);
		if(len > 1){
			buff[len] = '\0';

			arg0 = mystrtok((char *)buff, " \n\t");
			arg1 = mystrtok(NULL, " \n\t");
			arg2 = mystrtok(NULL, " \n\t");
			arg3 = mystrtok(NULL, " \n\t");

			if(arg0){
				if(!strcmp(arg0, "exit")){
					sync();
					printf("exit system ...\n");
					_exit(0);
				}

				if(!strcmp(arg0, "cd")){
					if(arg1 == NULL){
						printf("cd pathname\n");
						continue;
					}
					chdir(arg1);
					continue;
				}

				do_execve(arg0, arg1, arg2, arg3);
			}
		}else
			tcsetattr(0, TCSANOW, &def_termios);
	}

	return 0;
}

char * mystrtok(char * string, char * delim)
{
	if(string){
		int i, ibak, j, len;

		strcpy(arg_buff, string);

		arg_num = 0;
		arg_index = 0;

		i = 0;
		while((arg_buff[i] == ' ') || (arg_buff[i] == '\t'))
			i++;

		arg_ptr[arg_num++] = &arg_buff[i];

		len = strlen(arg_buff);

		for(ibak = i; arg_buff[i]; i++){
			for(j = 0; delim[j]; j++){
				if(arg_buff[i] == delim[j]){
					arg_buff[i] = '\0';
					break;
				}
			}
		}


		for(i = ibak + 1; i < len; i++){
			if(!arg_buff[i] && (i < len - 1) && arg_buff[i + 1]){
				arg_ptr[arg_num++] = &arg_buff[i + 1];
			}
		}

		return arg_ptr[arg_index++];
	}else{
		if(arg_index < arg_num)
			return arg_ptr[arg_index++];
		else	
			return NULL;
	}
}

void do_execve(char * filename, char * arg1, char * arg2, char * arg3)
{
	int i, pid, pid1, status;
	char * argvptr[5];

	i = 0;
	argvptr[i++] = filename;
	if(arg1 != NULL)
		argvptr[i++] = arg1;
	if(arg2 != NULL)
		argvptr[i++] = arg2;
	if(arg3 != NULL)
		argvptr[i++] = arg3;
	argvptr[i++] = NULL;

	if(!(pid = fork())){
		execvp(filename, argvptr);
		printf("exeve failed error: %s\n", strerror(errno));
		exit(0);
	}
	
	do{
		pid1 = wait(&status);
	}while(pid1 != pid);
}
