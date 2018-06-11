CC = /usr/cross/bin/i586-pc-cnix-gcc
CFLAGS = -Wall -DCNIX -O2

SOURCES := $(wildcard *.c)
OBJS := $(patsubst %.c, %.o, $(SOURCES))
EXES := $(patsubst %.c, %, $(SOURCES))

.SUFFIXES: .c .s

all : tetris tetris.s tetris.asm


tetris:	$(OBJS)
	$(CC) $(OBJS) -o $@
	objdump -d $@ > $@.S

%c%.o:
	$(CC) $(CFLAGS) -c -o $@ $< 

tetris.s: tetris.c
	$(CC) $(CFLAGS) -S -o $@ $<

tetris.asm: tetris.bin
	../../tools/ndisasm -b32  $< > $@

tetris.bin:tetris
	objcopy -O binary -R .comment -R .note $< $@

clean:
	rm -f *.o $(OBJS) tetris *.~* a.out debug.txt *.s *.bin *.map *.asm *.S
