$Id: readme.txt,v 1.1 2003/10/08 09:27:11 cnqin Exp $

These codes are to boot cnix from the harddisk. 
Boot0.s is the mbr code.
Boot2.s is the code to bring the cnix system into memory which is at 0x1000:0000.
write_mbr.c is to write mbr into disk.
write_boot.c is to write the boot code into the boot block in minixfs.
test-boot2.c is to test how to read file from minixfs.

NOTE:
   To be convient, I setup the disk's capicity is too small (5M).
So if you want to test the boot in your own virtual disk which the capacity
is not consistent with mine, you must setup yourself in param.h.



