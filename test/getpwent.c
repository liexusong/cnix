#include <stdio.h>
#include <pwd.h>
#include <stdlib.h>

int main(void)
{
	struct passwd * pwd;

	while((pwd = getpwent()))
		printf("name: %s\npassword: %s\nuid: %d\ngid: %d\ncomment: %s\nhome: %s\nshell: %s\n", pwd->pw_name, pwd->pw_passwd, pwd->pw_uid, pwd->pw_gid, pwd->pw_gecos, pwd->pw_dir, pwd->pw_shell);

	return 0;
}
