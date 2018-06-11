/*
 * boot.S
 * It is loaded at address 0x1000:0000 by the Boot2.
 * Modified by ps/2.
 */
 
 /* $Id: boot.S,v 1.2 2003/10/08 09:07:07 cnqin Exp $	*/


BOOTSEG = 0x07c0
SYSSEG	= 0x1000
SYSSIZE = 0x4000	# Max value may be 0x9000. 
SYSEND	= SYSSEG + SYSSIZE

CHANGE  = 0x200 + 0x23 + 2

.code16
.text
.global _start
_start:
	movw	$BOOTSEG, %ax
	movw	%ax, %ds
	movw	%ax, %es

	cli
	movw	%ax, %ss
	movw	$0x8000, %sp	# 0x8000 + 0x7c00 < 0x10000
	sti

# get extent memory size
	movb	$0x88,%ah
	int	$0x15
	movw	%ax,(2)

	movw	$SYSSEG, %ax
	movw	%ax, %es
	
	
# Save the current cursor at address 0x7c00 + 510, 0x7c00 + 511.
	movb	$3, %ah
	xorb	%bh, %bh
	int	$0x10
	movw	%dx, (510)

# Set the SYSSEG
	movw	$SYSSEG, 	%ax
	movw	%ax,	%ds
	movw	%ax,	%es
# When we boot the system from the harddisk , we must 
# modify the offset in head.S before the head.S have moved itself
# from the 0x10000 to 0x100000.
	movb	$0x2,	CHANGE

# Now it's time to enter protected mode.
	cli
	lidt	idt_pdesc
	lgdt	gdt_pdesc

# Enable a20.
	call	kb_wait
	movb	$0xd1, %al	# command write.
	outb	%al, $0x64
	call	kb_wait
	movb	$0xdf, %al	# enable a20.
	outb	%al, $0x60
	call	kb_wait

# Assume all things is ok. Go on to initialize 8259.
	movb	$0x11, %al	# initialize sequence.
	outb	%al, $0x20	
	call	delay		
	outb	%al, $0xa0	
	call	delay
	movb	$0x20, %al	# start of hardware interrupt (0x20).
	outb	%al, $0x21
	call	delay
	movb	$0x28, %al	# start of hardware interrupt (0x28).
	outb	%al, $0xa1
	call	delay
	movb	$0x04, %al	# set master.
	outb	%al, $0x21
	call	delay
	movb	$0x02, %al	# set slave.
	outb	%al, $0xa1
	call	delay
	movb	$0x01, %al	# 8086 mode.
	outb	%al, $0x21
	call	delay
	outb	%al, $0xa1
	call	delay
	movb	$0xfb, %al	# mask all.
	outb	%al, $0x21
	call	delay
	movb	$0xff, %al
	outb	%al, $0xa1
	
# Actually enter protected mode.
	movw	$0x0001, %ax
	lmsw	%ax
	.byte	0x66, 0xea
code386:.long	0x10200		# the next sector is head.S and system
	.word	8

kb_wait:
	.word	0x00eb, 0x00eb	# Dealy
	inb	$0x64, %al	
	testb	$0x02, %al	# Keyboard buffer is full?
	jnz	kb_wait		# Yes, wait.
delay:
	ret

gdt:
	.word	0, 0, 0, 0

	.word	0x07ff		# limit == 8M (2048 * 4096)
	.word	0x0000		# base address = 0
	.word	0x9a00		# code read/exec
	.word	0x00c0		# unit = 4096, 386

	.word	0x07ff
	.word	0x0000
	.word	0x9200		# data read/write
	.word	0x00c0
	
idt_pdesc:
	.word	0
	.word	0, 0
	
gdt_pdesc:
	.word	8 * 3 - 1	# gdt limit = 24, 3 GDT entries
	.word	gdt , 0x1	# 0x10000 + gdt

# to save memory size	
.org 506
	.word 0,0
.org 510
	.word 0xaa55
