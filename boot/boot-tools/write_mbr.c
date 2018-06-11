/*
 * $Id: write_mbr.c,v 1.3 2003/10/15 11:25:20 xiexiecn Exp $
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
	char buff[512];
	char *from , *to;
	int fdto, fdfrom;

	from = "boot0";
	to   = "hdimg64M";

	if (argc == 2){
		from = argv[1];
	}else if (argc == 3){
		from = argv[1];
		to = argv[2];
	}
	if ((fdfrom = open(from, O_RDONLY)) == -1){
		printf("open %s for reading failed!\n",from);
		return 1;
	}
	if ((fdto = open(to, O_RDWR)) == -1){
		printf("open %s for writing failed!\n", to);
		return 1;
	}
	if (read(fdfrom, buff, 512) != 512){
		printf("Read error!\n");
		return 1;
	}

	if (write(fdto, buff, 0x1BE) != 0x1BE){
		printf("Write Error!\n");
		return 1;
	}
	lseek(fdto, 510, SEEK_SET);
	write(fdto, &buff[510], 2);	/* write 0xAA55 */
	close(fdto);
	close(fdfrom);
	return 0;
		
}
/*
 * $Log: write_mbr.c,v $
 * Revision 1.3  2003/10/15 11:25:20  xiexiecn
 * hdimg5M-->hdimg64M
 *
 * Revision 1.2  2003/10/08 09:44:10  cnqin
 * Add $id$ and $log$ keyword
 *
 */
