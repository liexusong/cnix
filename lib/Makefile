AR = ar
AS = as
LD = ld
CC = gcc
CFLAGS = -Wall 

OBJS = start.o

all: $(OBJS)

%.o%c:
	$(CC) $(CFLAGS) -c $< -o $@

%.o%S:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o
