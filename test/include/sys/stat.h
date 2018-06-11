#ifndef _STAT_H
#define _STAT_H

#include <sys/types.h>

/* bits for i_mode */
#define I_TYPE	0170000	/* inode type mode */
#define I_REG	0100000	/* regular file */
#define I_BLK	0060000	/* block special file */
#define I_DIR	0040000	/* directory */
#define I_CHR	0020000	/* character special file */
#define I_FIFO	0010000 /* named pipe */

#define S_ISREG(m)	((m & I_TYPE) == I_REG)
#define S_ISBLK(m)	((m & I_TYPE) == I_BLK)
#define S_ISDIR(m)	((m & I_TYPE) == I_DIR)
#define S_ISCHR(m)	((m & I_TYPE) == I_CHR)
#define S_ISFIFO(m)	((m & I_TYPE) == I_FIFO)

#define S_IFMT		I_TYPE
#define S_IFREG		I_REG
#define S_IFBLK		I_BLK
#define S_IFDIR		I_DIR
#define S_IFCHR		I_CHR
#define S_IFFIFO	I_FIFO

#define I_SU	0004000	/* set effective uid on exec */
#define I_SG	0002000	/* set effective gid on exec */

#define I_UGRWX	0006777

#define I_RWX	0000777
#define I_RB	0000004
#define I_WB	0000002
#define I_XB	0000001
#define I_FREE	0000000

#define	S_IRWXU 	(S_IRUSR | S_IWUSR | S_IXUSR)
#define		S_IRUSR	0000400	/* read permission, owner */
#define		S_IWUSR	0000200	/* write permission, owner */
#define		S_IXUSR 0000100/* execute/search permission, owner */
#define	S_IRWXG		(S_IRGRP | S_IWGRP | S_IXGRP)
#define		S_IRGRP	0000040	/* read permission, group */
#define		S_IWGRP	0000020	/* write permission, grougroup */
#define		S_IXGRP 0000010/* execute/search permission, group */
#define	S_IRWXO		(S_IROTH | S_IWOTH | S_IXOTH)
#define		S_IROTH	0000004	/* read permission, other */
#define		S_IWOTH	0000002	/* write permission, other */
#define		S_IXOTH 0000001/* execute/search permission, other */

#define		S_IEXEC		S_IXUSR
#define		S_IFLNK		0120000
#define		S_IREAD		0000400
#define		S_IWRITE	0000200
#define		S_ISUID		0004000
#define		S_ISGID		0002000
#define		S_ISVTX		0001000
#define		S_IFIFO		0010000

struct stat
{
	unsigned long  st_dev;
	unsigned long  st_ino;
	unsigned short st_mode;
	unsigned short st_nlink;
	unsigned short st_uid;
	unsigned short st_gid;
	unsigned long  st_rdev;
	unsigned long  st_size;
	unsigned long  st_blksize;
	unsigned long  st_blocks;
	unsigned long  st_atime;
	unsigned long  st_atime_nsec;
	unsigned long  st_mtime;
	unsigned long  st_mtime_nsec;
	unsigned long  st_ctime;
	unsigned long  st_ctime_nsec;
	unsigned long  __unused4;
	unsigned long  __unused5;

};

extern int stat(const char * path, struct stat * buf);
extern int mkdir(const char * pathname, mode_t mode);
extern int rmdir(const char * pathname);

#endif
