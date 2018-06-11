/*
 * This code is loaded by bios's service int19 at 0000:7c00.
 * It is moved from 0000:0x7c00 to 0000:0x6000. 
 * Then it finds the active partition ,and load the boot into 0000:0x7c00
 */

 /*
  * $Id: boot0.s,v 1.2 2003/10/08 09:44:10 cnqin Exp $
  */
BOOTSEG = 0X07C0
NEWSEG  = 0X0600

.code16
.text
.global start
start:
	movw 	$BOOTSEG, %ax
	movw	%ax,	%ds
	cli	
	movw	%ax,	%ss	# set up the stack pointer to an safe point
	movw	$0x8000, %sp
	sti	
	movw	$NEWSEG, %ax
	movw	%ax,	%es
	pushw	%ax		# then new address of code 0x600:newcode
	movw	$newcode - start, %bx
	pushw 	%bx	
	mov	$0x100, %cx	# 256 words
	cld
	# si = di = 0
	xorw	%si, 	%si
	xorw	%di,	%di
	repe	movsw		# transfer the data from 0x7c0:00 to 0x600:00
	lret			# jmp to 0x600:newcode
newcode:
	# find the active partition
	# now DS = SS = 0x7C0 and CS = ES = 0x600
	pushw	%cs
	popw	%ds		# DS = CS = ES = 0x600 , SS = 0x7c0
	movb	$0x4,	%cl
	xorw	%bp,	%bp
	movw	$0x1BE,	%si	# Si points to the first entry
next:	
	movb	(%si),	%al
	cmpb	$0x0,  %al
	jge	1f
	orw	%bp,	%bp
	jnz	1f		# has found , only skip it
	movw	%si,	%bp
1:
	addw	$16,	%si	# next partition entry
	decb	%cl
	orb	%cl,	%cl
	jnz	next
	orw	%bp,	%bp
	jnz	load
	# it dosen't find the active partition, print some error message
	movw	$no_active_msg, %si
	jmp	display
load:
	# now bp point to the active partition table
	movw	%bp,	%si
	movw	$0x5,	%bp	# the number to be tried
1:	
	movw	$0x0202,%ax	# ah = 02h al = 02 read two sectors
	movw	2(%si),	%cx	# ch = cylinder, cl = sector
	movb	1(%si),	%dh
	movb	$0x80,	%dl	# the first harddisk
	pushw	%ss
	popw	%es		# es:bx = 0x7c0:0000
	xorw	%bx,	%bx
	int	$0x13
	jnc	check_sig	# no errors
	xorw	%ax,	%ax
	int	$0x13		# reset harddisk
	decw	%bp
	orw	%bp,	%bp
	jnz	1b
check_ext:	
	# now  we try to use extended harddisk call
	movb	$0x41,	%ah
	movw	$0x55AA,	%bx
	movb	$0x80,	%dl
	int	$0x13
	jnc	1f
	movw	$no_sup_ext_hard, %si
	jmp	display
1:
	andb	$0x20,	%ah
	jnz	1f
	# version is too low
	movw	$err_ver_low,	%si
	jmp	display
1:
	cmpw	$0xAA55, %bx
	jz	1f
	movw	$no_sup_ext_hard, %si
	jmp 	display
1:
	# construct Disk Address Packet
	# BYTE PacketSize, BYTE Reserved, WORD NumberOfSectors 
	# DWORD BufferAddress, QWORD AbsoluteSectorBlockNumber
	pushw	%si		# Si point to the active partition entry , must save it
	movw	$BOOTSEG, %ax	# to test 
	movw	%ax,	%es	
	xorl	%eax,	%eax
	pushl	%eax
	pushl	8(%si)		# the number of starting block
	pushw	%es
	pushw	%ax		# 0x7C0:0000
	movw	$2,	%ax	# now we must read two sectors
	pushw	%ax		# one sector
	movw	$0x10,	%ax
	pushw	%ax		# Reserved and Size
	movb	$0x42,	%ah
	movb	$0x80,	%dl
	movw	%sp,	%si
	pushw	%ds		# save the original DS
	pushw	%ss
	popw	%ds
	int	$0x13
	popw	%ds		# restore DS
	addw	$0x10,	%sp	# balance statck, can not take carry
	popw	%si		# resotre active partition entry
	jnc	check_sig
	movw	$hard_msg, %si
	jmp	display
check_sig:
	cmpw	$0xA5A5, %es:1022	# ES = 0x7c0
	jz	goto_boot1
	movw	$no_sig, %si
	jmp	display
goto_boot1:
	pushw	%es
	xorw	%di, %di
	pushw	%di
	lret		#goto boot1
	# display the error msg 
	# entry: ds:si ==> the msg's address
display:
	movw	$NEWSEG, %bx
	movw	%bx,	%ds
	movb	$0xe,	%ah
	movw	$0x07,	%bx	# page# and foreground color
1:	
	lodsb
	orb	%al,	%al
	jz	die
	int	$0x10
	jmp	1b
die:
	jmp	die

no_active_msg:
	.asciz	"No active partition!"
hard_msg:
	.asciz	"The harddisk can not be read!"
no_sig:
	.asciz	"Boot1 dosesn't have signature 0xAA55!"
no_sup_ext_hard:
	.asciz	"The extention isn't supported!"
err_ver_low:
	.asciz	"The version is too low!"

.org	0x1BE
	.fill	64, 1, 0	
.org	510	# magic
	.word	0xAA55
/*
 * $Log: boot0.s,v $
 * Revision 1.2  2003/10/08 09:44:10  cnqin
 * Add $id$ and $log$ keyword
 *
 */
