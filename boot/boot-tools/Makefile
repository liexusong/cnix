#
# $Id: Makefile,v 1.3 2003/10/15 12:29:24 xiexiecn Exp $
#
CFLAGS = -g -Wall
CC	= gcc
AS	= as
ASFLAGS = --fatal-warnings -a -a=$@.lis

hdimg.zip:boot0 boot2 write_mbr write_boot hdimg64M
	./write_mbr
	./write_boot
	zip hdimg.zip hdimg64M
boot0:boot0.o
	objcopy -S -O binary $< $@
boot2:boot2.o
	objcopy -S -O binary $< $@
boot2.o:boot2.s
	$(AS) $(ASFLAGS) -o $@ $<
boot0.o:boot0.s
	$(AS) $(ASFLAGS) -o $@ $<
.c.o:	
clean:
	rm -f boot0 boot0.o write_mbr write_boot boot2.o boot2 *.lis

#
# $Log: Makefile,v $
# Revision 1.3  2003/10/15 12:29:24  xiexiecn
# *** empty log message ***
#
# Revision 1.2  2003/10/08 09:44:10  cnqin
# Add $id$ and $log$ keyword
#
#
