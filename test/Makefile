CC = /usr/cross/bin/i586-pc-cnix-gcc
CFLAGS = -Wall -O2

SOURCES := $(wildcard *.c)
OBJS := $(patsubst %.c, %.o, $(SOURCES))
EXES := $(patsubst %.c, %, $(SOURCES))

all:	$(EXES) tetris/tetris ed/myed
	cp $(EXES) tetris/tetris ed/myed tmp

$(EXES): %: %.o
	$(CC) $@.o -o $@

tetris/tetris: tetris/*.c tetris/*.h
	make -C tetris

ed/myed: ed/*.c ed/*.h
	make -C ed
	
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 

clean:
	rm -f *.o $(EXES) tmp/*
	(cd ../lib; make clean)
	(cd tetris; make clean)
	(cd ed; make clean)
