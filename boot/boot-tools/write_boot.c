/*
 * $Id: write_boot.c,v 1.4 2003/10/15 11:25:49 xiexiecn Exp $
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "param.h"

int read_mbr(char *buff, const char *path);
struct partition * find_boot_part(struct partition *start);
int write_boot1(const struct partition *pe, const char *boot1, const char *to);
int write_boot2(const struct partition *pe, const char *boot2 , const char *to);
int check_part_entry(const struct partition *pe);
#define MAGIC 0xAA55
#define PART_OFFSET 0x1BE

int main(int argc, char *argv[])
{
	char buff[512];
	struct partition *pe;
	
	char *boot1, *boot2, *to;

	to = "hdimg64M";
	boot1 = "boot1";
	boot2 = "boot2";

	if (argc == 2)
		to = argv[1];
	
	if (!read_mbr(buff, to)){
		printf("Can not read mbr: %s\n", to);
		return 1;
	}
	
	pe = find_boot_part((struct partition *)(buff + PART_OFFSET));
	if (!pe){
		printf("Can not find active partition!\n");
		return 1;
	}
#if 0
	if (!write_boot1(pe, boot1 , to)){
		printf("Can not write boot1!\n");
		return 1;
	}
#endif	
	if (!write_boot2(pe, boot2, to)){
		printf("Can not write boot2!\n");
		return 1;
	}
	printf("Success!\n");
	return 0;
}

int 
read_mbr(char *buff, const char *path)
{
	int fd;
	
	fd = open(path, O_RDONLY);
	if (fd == -1)
		return 0; //fail
	if (read(fd, buff, 512) != 512)
		return 0;
	close(fd);
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

int write_boot1(const struct partition *pe, const char *boot1, const char *to)
{
	int fdf, fdt;
	long offset;
	char buff[512];
	int  ret = 0;

	fdf = fdt = -1;		// initial value
	if (!check_part_entry(pe))
		return 0;

	offset = pe->start_sec * 512;
	fdt = open(to, O_RDWR);
	if (fdt == -1)
		goto error;
	fdf = open(boot1, O_RDONLY);
	if (fdf == -1)
		goto error;
	if (lseek(fdt, offset, SEEK_SET) == -1)
		goto error;
	if (read(fdf, buff, 512) != 512)
		goto error;
	buff[510] = 0x55;
	buff[511] = 0xAA;
	if (write(fdt, buff, 512) != 512){
		perror("Error");
		goto error;
	}
	ret = 1;		// success
error:
	if (fdt != -1)
		close(fdt);
	if (fdf != -1)
		close(fdf);
	return ret;
}

/*
 * Now the boot2 resident at the first block in the minix fs (BOOT_BLOCK)
 */
int write_boot2(const struct partition *pe, const char *boot2 , const char *to)
{
	int fdf, fdt;
	long offset;
	unsigned char buff[1024];
	int  ret = 0, n;

	fdf = fdt = -1;		// initial value
	offset = (pe->start_sec) * 512; /* boot2, is consective after boot1 */
	fdt = open(to, O_RDWR);
	if (fdt == -1)
		goto error;
	fdf = open(boot2, O_RDONLY);
	if (fdf == -1)
		goto error;
	if (lseek(fdt, offset, SEEK_SET) == -1){
		perror("lseek fdt error!\n");
		goto error;
	}
	if (lseek(fdf, -2L, SEEK_END) == -1){
		perror("lseek fdf error!\n");
		goto error;
	}
	if (read(fdf, buff, 2) != 2)
		goto error;
	/* check the signature */
	if (buff[0] != 0xA5 || buff[1] != 0xA5){
		printf("No signature!\n");
		goto error;
	}
	lseek(fdf, 0, SEEK_SET);

	n = read(fdf, buff, 1024);
	if (n != 1024)
		goto error;
	if (write(fdt, buff, n) != n){
		printf("write to fdt error!\n");
		goto error;
	}
	ret = 1;		// success
error:
	if (fdt != -1)
		close(fdt);
	if (fdf != -1)
		close(fdf);
	return ret;
}
/*
 * $Log: write_boot.c,v $
 * Revision 1.4  2003/10/15 11:25:49  xiexiecn
 * hdimg5M-->hdimg64M
 *
 * Revision 1.3  2003/10/08 09:44:10  cnqin
 * Add $id$ and $log$ keyword
 *
 */
