all:	.tmp.o	version.c

OBJS := $(patsubst %.c, %.o, $(wildcard *.c))
OBJS += $(patsubst %.S, %.o, $(wildcard *.S))
OBJS += version.o

.tmp.o:	$(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	rm -f version.*

%.o: %.c
	$(CC) $(CFLAGS) -c $<
%.o: %.S
	$(CC) $(CFLAGS) -c $<

# generate the version info
.PHONY: version.c
export DATE=$(shell date "+%F %T")
export HOST=$(shell hostname)
version.c: 
	echo -e "#include \"cnix/verno.h\"\n"  > $@
	echo -e "const char version[] = { VERNO \" @$$HOST, $$DATE\\\\n\" };" >> $@

clean:
	rm -f *.o .*.swp *~ .tmp.o version.c
