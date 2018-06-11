CC = gcc
CFLAGS = -g

all: makeimg genmap compccmap

makeimg: makeimg.c
	$(CC) $(CFLAGS) -o makeimg makeimg.c

genmap: genmap.c
	$(CC) $(CFLAGS) -o genmap genmap.c

compccmap: compccmap.c
	$(CC) $(CFLAGS) -o compccmap compccmap.c

clean:
	rm -f *.o makeimg genmap compccmap
	rm -f *~
	rm -f .*.swp
