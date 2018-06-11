#ifndef _FS_H
#define _FS_H

#include <cnix/limits.h>
#include <cnix/buffer.h>
#include <cnix/inode.h>
#include <cnix/super.h>
#include <cnix/file.h>
#include <cnix/direct.h>
#include <cnix/stat.h>
#include <cnix/time.h>

#define SU_UID	0
#define SU_GID	0

#define MAJOR(dev)	((dev >> 8) & 0xff)
#define MINOR(dev)	(dev & 0xff)

#define ROOT_DEV	(3 << 8 | 1)

#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

#define ckfdlimit(fd) ((fd < 0) || (fd >= OPEN_MAX))

/* buffer function */
extern struct buf_head * getblk(int, int);
extern struct buf_head * bread(int ,int);
extern void brelse(struct buf_head *);
extern void bwrite(struct buf_head *);
extern void iodone(struct buf_head *);

/* file system init function */
extern void fs_init(void);
extern void init_super(void);
extern void init_inode(void);
extern void init_filp(void);

/* do_syscall */
extern int do_creat(const char *, mode_t);
extern int do_open(const char *, int, mode_t);
extern int do_inode_open(struct inode *, int);
extern int do_mknod(const char *, mode_t, dev_t);
extern int do_pipe(int [2]);

extern ssize_t do_read(int, char *, size_t);
extern ssize_t do_write(int, const char *, size_t);
extern off_t do_lseek(int, off_t, int);
extern int do_close(int);

extern int do_stat(const char *, struct stat *);
extern int do_lstat(const char *, struct stat *);
extern int do_fstat(int, struct stat *);
extern int do_dup0(int, int);
extern int do_dup(int);
extern int do_dup2(int, int);

extern int do_link(const char *, const char *);
extern int do_unlink(const char *);
extern int do_rename(const char *, const char *);
extern int do_symlink(const char *, const char *);
extern int do_readlink(const char *, char *, size_t);

extern int do_mount(const char *, const char *, int);
extern int do_umount(const char *);

extern int do_chdir(const char *);
extern int do_chroot(const char *);

extern int do_mkdir(const char *, mode_t);
extern int do_rmdir(const char *);
extern int do_getcwd(char *, size_t);

extern int do_access(const char *, int);
extern int do_chmod(const char *, mode_t);
extern int do_chown(const char *, uid_t, gid_t);
extern int do_utime(const char *path, const struct utimbuf *buf);

/* internal function for syscall */
extern int admit(struct inode *, mode_t);
extern block_t bmap(struct inode *, off_t);
extern int minode(struct inode *, off_t, zone_t);
extern struct inode * ifind(struct inode *, const char *);
extern int iempty(struct inode *);
extern struct inode * iinsert(struct inode *,
	const char *, mode_t, int *, ino_t);
extern int idelete(struct inode *, const char *, int *);
extern struct inode * namei0(const char *, int *, int);
extern struct inode * namei(const char *, int *, int);
extern int bypass(const char *, int *, int, int, struct inode *);
extern int get_tail(const char *, char *);
extern int get_name_of_son(struct inode *, ino_t, char *, int);
extern void truncate(struct inode *);
extern size_t calc_block_nr(size_t length);
#endif
