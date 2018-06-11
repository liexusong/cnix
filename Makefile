# Makefile for cnix

PWD = $(shell pwd)

AS := as
LD := ld
CC := gcc

LDFLAGS := -r
CFLAGS	= -Wall -O2 -fstrength-reduce \
	 -finline-functions -fno-builtin -nostdinc -nostdlib -nodefaultlibs \
	 -I../include
MKFLAGS := -C

export AS LD CC LDFLAGS CFLAGS CPP MKFLAGS

all:	fdimg

SUBDIRS := driver net mm fs kernel init

ARCHIVES := $(patsubst %, %/.tmp.o, $(SUBDIRS))

fdimg:	boot/bootsect tools/system
	objcopy -O binary -R .note -R .comment tools/system tools/kernel
	tools/makeimg boot/bootsect tools/kernel $@
	tools/ndisasm -b32 tools/kernel > System.asm
	rm -f tools/kernel
	sync

hdimg:	boot/boot tools/system
	objcopy -O binary -R .note -R .comment tools/system tools/kernel
	cat boot/boot tools/kernel > $@
	rm -f tools/kernel
	sync

boot/bootsect: boot/bootsect.o
	(cd boot; make bootsect)

boot/boot: boot/boot.o
	(cd boot; make boot)

boot/head.o: boot/head.S
	(cd boot; make head.o)

tools/system: $(ARCHIVES) boot/head.o
	$(LD) -T cnix.lds boot/head.o $(ARCHIVES) -o tools/system 
	nm tools/system | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aU] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)'| sort > System.map

.PHONY:	$(ARCHIVES)
$(ARCHIVES): $(SUBDIRS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	make $(MKFLAGS) $@

clean:
	rm -f fdimg hdimg System.map System.asm tools/system
	(cd boot; make clean)
	(cd init;make clean) 
	(cd driver;make clean)
	(cd driver; make clean)
	(cd mm;make clean)
	(cd fs;make clean)
	(cd kernel;make clean)
	(cd net; make clean)
	(cd include/asm;rm -f .*.swp *~)
	(cd include/cnix;rm -f .*.swp *~)
