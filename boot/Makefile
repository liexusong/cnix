all:	bootsect boot head.o

bootsect: 	bootsect.o
	$(LD) -Ttext 0x0 -s --oformat binary -o $@ $<
boot:	boot.o
	$(LD) -Ttext 0x0 -s --oformat binary -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f bootsect boot *.o  .*.swp *~
