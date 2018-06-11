#include <stdio.h>
#include <pwd.h>
#include <stdlib.h>

int main(int argc, char * argv[])
{
	struct passwd * pwd;
	if(argc < 2){
		printf("getuserinfo username|uid\n");
		exit(-1);
	}

	pwd = getpwnam(argv[1]);

	if(!pwd){
		printf("not found %s\n", argv[1]);
		return 0;
	}

	printf("name: %s\npassword: %s\nuid: %d\ngid: %d\ncomment: %s\nhome: %s\nshell: %s\n", pwd->pw_name, pwd->pw_passwd, pwd->pw_uid, pwd->pw_gid, pwd->pw_gecos, pwd->pw_dir, pwd->pw_shell);

	return 0;
}
