CC = /usr/cross/bin/i586-pc-cnix-gcc
#CC=gcc
CFLAGS = -Wall -O2
LD = ld

OBJS = ed.o misc.o cmd.o 

myed:	$(OBJS)
	$(CC) -o $@ $^
	objdump -d $@ > $@.S

%c%.o: ed.h misc.h
	$(CC) $(CFLAGS) -c -o $@ $< 

clean:
	rm -f *.o $(OBJS) myed *.~* a.out *.S *.s *.asm core
