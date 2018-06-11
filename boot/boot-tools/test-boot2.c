/*
 * $Id: test-boot2.c,v 1.4 2003/10/15 11:24:38 xiexiecn Exp $
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "param.h"

#define BLOCKSIZ	1024
#define NR_ZONES	10
#define NR_DZONES	7
#define NR_INDIRECTS	(BLOCKSIZ / sizeof(unsigned long))

#define S_MAGIC		0x2478

struct super_block{
	unsigned short s_ninodes;	/* total inodes'number */
	unsigned short s_nzones;     /* V1 total zones'number, include bitmap*/
	unsigned short	s_imblocks;	/* inode map blocks */
	unsigned short	s_zmblocks;	/* zone map blocks */
	unsigned short	s_fdzone;	/* first data zone number */
	unsigned short	s_lzsize;	/* log of zone size */
	unsigned int	s_msize;	/* max file size */
	unsigned short	s_magic;	/* magic number */
	unsigned short	s_pad;		/* padding */
	unsigned int	s_zones;	/* V2 total zones'number, include bitmap */
};
struct inode{
	unsigned short	i_mode;
	unsigned short	i_nlinks;
	unsigned short	i_uid;
	unsigned short	i_gid;
	unsigned int 	i_size;
	unsigned int	i_atime;
	unsigned int	i_mtime;
	unsigned int	i_ctime;
	unsigned int	i_zone[NR_ZONES];
};



#define BOOT_BLOCK	0
#define SUPER_BLOCK	(BOOT_BLOCK + 1)
#define IMAP_BLOCK	(SUPER_BLOCK + 1)	
	

void print_img_blocks();
unsigned int bmap(unsigned int off);
void read_super_block();
void find_img_inode();
void read_inode(unsigned int ino);
unsigned int find_img_ino(struct inode *ip);
unsigned int find_entry(unsigned int blocknr);
void read_block(unsigned int blocknr, char buff[]);
void init_hd(const char *hd);
int read_mbr(char *buff, const char *path);
struct partition * find_boot_part(struct partition *start);
int check_part_entry(const struct partition *pe);


struct super_block super;
struct inode	inode;
unsigned int	starting_sect;

const char *img="kernel";
int fd;

int main(int argc, char *argv[])
{
	const char *hd="hdimg64M";

	init_hd(hd);
	read_super_block();
	find_img_inode();
	print_img_blocks();
	return 0;
}

void print_img_blocks()
{
	unsigned int blocknr;
	unsigned int off;
	char buff[BLOCKSIZ];
	int i;
	
	off = 0;
	while (off < inode.i_size){
		blocknr = bmap(off);
		read_block(blocknr, buff);
		for (i = 0; i < BLOCKSIZ && i < (inode.i_size - off); i++)
			printf("%c", buff[i]);
		off += i;
	}


}

#define POINTER_PER_BLOCK	((BLOCKSIZ / sizeof(unsigned int)))
unsigned int bmap(unsigned int off)
{
	unsigned int blocknr = off  >> 10;
	unsigned char buff[BLOCKSIZ];

	if (blocknr < NR_DZONES)
		return inode.i_zone[blocknr];
	blocknr -= NR_DZONES;
	if (blocknr < POINTER_PER_BLOCK){ /* at the indirect block*/
		read_block(inode.i_zone[NR_DZONES], buff);
		if (((unsigned int *)buff)[ blocknr ])
			return ((unsigned int *)buff)[blocknr];
		else {
			printf("Warning: pointer in datablock is ZERO!\n");
			return 0;
		}
	}
	blocknr -= POINTER_PER_BLOCK;
	if (blocknr > POINTER_PER_BLOCK * POINTER_PER_BLOCK){
		printf("File is too large!\n");
		return 0;
	}

	read_block(inode.i_zone[NR_DZONES + 1], buff);
	{
		char data[BLOCKSIZ];
		read_block(((unsigned int *)buff)[ blocknr / POINTER_PER_BLOCK],
				data);
		if (((unsigned int *)data)[ blocknr % POINTER_PER_BLOCK ])
			return ((unsigned int *)data)[ blocknr % POINTER_PER_BLOCK ];
		else {
			printf("in dindirect datablock is ZERO!\n");
			return 0;
		}
	}
}	
	

void read_super_block()
{
	char buff[BLOCKSIZ];

	read_block(SUPER_BLOCK, buff);
	super = *((struct super_block *)buff);
	if (super.s_magic != S_MAGIC){
		printf("The superblock dose not have magic!\n");
		exit(1);
	}
}
void find_img_inode()
{
	struct inode *ip;
	char buff[BLOCKSIZ];
	unsigned int ino;

	read_block(IMAP_BLOCK + super.s_imblocks + super.s_zmblocks, buff);
	ip = (struct inode *)buff;
	ino = find_img_ino(ip);
	if (ino == 0){
		printf("Not find image:%s\n",img);
		exit(1);
	}
	read_inode(ino);
}
#define INODE_PER_BLOCK 	(BLOCKSIZ / sizeof(struct inode))
void read_inode(unsigned int ino)
{
	char buff[BLOCKSIZ];

	read_block(IMAP_BLOCK + super.s_imblocks + super.s_zmblocks + \
			(ino - 1) / INODE_PER_BLOCK, buff);
	inode = ((struct inode *)buff)[ (ino - 1) % INODE_PER_BLOCK ];
}
	

unsigned int find_img_ino(struct inode *ip)
{
	int i;
	char buff[BLOCKSIZ];
	unsigned int ret;

	for (i = 0; i < NR_DZONES && ip->i_zone[i]; i++){
		ret = find_entry(ip->i_zone[i]);
		if (ret)
			return ret;
	}
	/* find in the indirect block */
	if (ip->i_zone[NR_DZONES] == 0)
		return 0;
	read_block(ip->i_zone[NR_DZONES], buff);
	for (i = 0; i < POINTER_PER_BLOCK; i++)
		if ((ret = find_entry(((unsigned int *)buff)[i])))
			return ret;
	if (ip->i_zone[NR_DZONES + 1] == 0)
		return 0;
	read_block(ip->i_zone[NR_DZONES + 1], buff);
	for (i = 0; i < POINTER_PER_BLOCK; i++){
		char data[BLOCKSIZ];
		int j;
		read_block(((unsigned int *)buff)[i], data);
		for (j = 0; j < POINTER_PER_BLOCK; j++)
			if ((ret = find_entry((unsigned int)data[j])))
				return ret;
	}

	return 0;
}
#define NAME_MAX 30
struct direntry{
	unsigned short d_ino;
	char d_name[NAME_MAX];
};
unsigned int find_entry(unsigned int blocknr)
{
	char buff[BLOCKSIZ];
	struct direntry *de;

	read_block(blocknr, buff);

	for (de = (struct direntry *)buff; (char *) de < (buff + BLOCKSIZ); de++){
		if (strncmp(de->d_name, img, NAME_MAX) == 0)
			return de->d_ino;
	}
	return 0;		/* not found */
}
	
	


void read_block(unsigned int blocknr, char buff[])
{
	unsigned int off;

	off = (starting_sect + (blocknr << 1)) * 512;

	if (lseek(fd, off, SEEK_SET) == -1){
		perror("lseek error!\n");
		exit (1);
	}
	if (read(fd, buff, BLOCKSIZ) != BLOCKSIZ){
		perror("Read block error!\n");
		exit(1);
	}
}

#define PART_OFFSET 0x1BE

void init_hd(const char *hd)
{
	struct partition *pe;
	unsigned char buff[512];
	
	if (!read_mbr(buff, hd)){
		printf("Can not read mbr: %s\n", hd);
		exit(1);
	}
	
	pe = find_boot_part((struct partition *)(buff + PART_OFFSET));
	if (!pe){
		printf("Can not find active partition!\n");
		exit(1);
	}
	check_part_entry(pe);
}

int 
read_mbr(char *buff, const char *path)
{
	fd = open(path, O_RDONLY);
	if (fd == -1)
		return 0; //fail
	if (read(fd, buff, 512) != 512)
		return 0;
	return 1; //sucess
}

struct partition * find_boot_part(struct partition *start)
{
	struct partition *p;

	for (p = start; p < &start[4]; p++){
		if (p->bootind == 0x80)
			return p;
	}
	return NULL;
}
/*
 * $Log: test-boot2.c,v $
 * Revision 1.4  2003/10/15 11:24:38  xiexiecn
 * hdimg5M-->hdimg64M
 *
 * Revision 1.3  2003/10/08 09:44:10  cnqin
 * Add $id$ and $log$ keyword
 *
 */
