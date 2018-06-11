all:	.tmp.o

OBJS := $(patsubst %.c, %.o, $(wildcard *.c))
OBJS += $(patsubst %.S, %.o, $(wildcard *.S))

.tmp.o:	$(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<
%.o: %.S
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o .*.swp *~ .tmp.o
