#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/types.h>
#include <cnix/limits.h>

struct dirent{
	ino_t d_ino;
	char d_name[NAME_MAX];
};

#define DIRENT_SIZE	sizeof(struct dirent)

typedef struct DIR_struct{
	int fd;
}DIR;

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

extern DIR * opendir(const char * dirname);
extern struct dirent * readdir(DIR * dir);
extern void closedir(DIR * dir);

#endif
