/*
 * This code residents at the boot block of minix fs to boot the cnix 
 * system. This code is loaded by Boot0 at 0x7C0:0000. Then it moves
 * itself to the 0x9B00:0000. And loads the System kernel at 0x1000:0000
 * This code is limited to 0x400 (1k)
 */

/*
 * $Id: boot2.s,v 1.2 2003/10/08 09:44:10 cnqin Exp $
 */

SETUPSEG	= 0X9B00
BOOTSEG		= 0X7C0
SYSSEG		= 0X1000
SUPER		= 0X500	# superblock
BUFFER		= 0X700	# buffer 
S_MAGIC		= 0X2478
ROOTI		= 1

/* offset in super_block */
OFF_S_MAGIC 	= 16
OFF_S_IMBLOCKS	= 4
OFF_S_ZMBLOCKS	= 6

/* offset in inode */
OFF_I_ZONE	= 24
OFF_I_SIZE	= 8



BOOT_BLOCK 	= 0
SUPER_BLOCK 	= BOOT_BLOCK + 1
IMAP_BLOCK	= SUPER_BLOCK + 1

INODES_PER_BLOCK = 16
NR_DZONES	= 7

.code16
.global start
.text
# When the Boot0 load Boot1 to 0x7C0:0000, DS:SI point the active
# parition entry (0x600:SI), So we can get some parameters from 
# DS:SI. And now , ES = SS = CS = 0x7C0, DS = 0x600


start:
	movl	8(%si), %eax
	movl	%eax,	%es:start_of_sect
	movl	12(%si), %eax
	movl	%eax,	%es:nr_of_sect
	# read the super block into memory
	pushw	%es
	popw	%ds
	movw	$SETUPSEG,	%ax
	movw	%ax,	%es
	cli
	movw	%ax,	%ss
	movw	$0x4000,	%sp
	sti
	pushw	%ax
	xorw	%si,	%si
	xorw	%di,	%di
	cld
	movw	$0x400,	%cx		# 1k
	repz	movsb
	movw	$new,	%ax
	pushw	%ax
	lret				# jmp $0x9B00:new
new:
	pushw	%es
	popw	%ds			# set ds to data
	# read superblock into superblock
	movl	$SUPER_BLOCK,	%eax

	movw	$SUPER,	%bp		# super's buffer
	call	read_block
	jnc	1f
	movw	$err_not_read_super,	%si
	jmp	display
1:
	cmpw	$S_MAGIC, OFF_S_MAGIC(%bp) # check the magic
	jz	1f
	movw	$err_no_magic,	%si
	jmp	display
1:
	# ok now we will read the root inode into memory
	xor	%eax, %eax
	movw	OFF_S_IMBLOCKS(%bp),	%ax
	addw	OFF_S_ZMBLOCKS(%bp),	%ax
	addw	$IMAP_BLOCK,	%ax
	movw	$BUFFER,	%bp		# buffer
	call	read_block
	jnc	1f
	movw	$err_not_read_root_inode,	%si
	jmp	display
1:
	movw	OFF_I_SIZE(%bp),	%cx	# the size of root directory
	addw	$1023,	%cx
	shrw	$10,	%cx			# covert it to the block
	cmpw	$NR_DZONES,	%cx
	jb	1f
	movw	$err_root_too_many_entries,	%si
	jmp	display
	
	# assume that it dosen't exceed NR_DZONES * BLOCKSIZ / 32 = 32 * 7
1:
	movw	$OFF_I_ZONE,	%si
next_zone:
	pushw	%cx
	movl	(%bp, %si),	%eax	# ASSUME THE BLOCK NUMBER ONLY AT THE
					# THE FIRST 64M
	call	read_block
	jnc	1f
	movw	$err_read_root_dir,	%si
	jmp	display
1:
	call	find_dir_entry		# ax is the inode number
	jc	find
	addw	$4,	%si		# read next zone
	popw	%cx			# restore the counter
	loop	next_zone
	movw	$err_no_img,	%si
	jmp	display
find:	
	# This code block is to read_inode()
	# ax is the img's inode number (the find_dir_entry's returned value)
	decw	%ax			# from 1
	xorw	%dx,	%dx
	movw	$INODES_PER_BLOCK, %bx
	divw	%bx
	# reminder in DX . Data block starts from IMAP_BLOCK + IMBLOCKS + ZMBLOCKS
	addw	$IMAP_BLOCK,	%ax
	addw	SUPER + OFF_S_IMBLOCKS , %ax
	addw	SUPER + OFF_S_ZMBLOCKS , %ax
	# now ax is the block number of which img's inode is in
	#  allocate memory in stack
	cli
	subw	$1024,	%sp
	movw	%sp,	%bp
	sti
	call	read_block
	xorw	%ax,	%ax
	movb	$64,	%al		# inode's size
	mul	%dl			# offset 
	# ax is the offset of the inode 
	addw	%ax,	%bp		
	movw	%bp,	%si		# the inode of img
	movw	%si,	%di		# di always point to the inode 
	movw	$SYSSEG, %bp
	movw	%bp,	%es
	xorw	%bp,	%bp		# into SYSSEG:0000
	addw	$OFF_I_ZONE,	%si
	# dx:bx	 is the postition
	xorw	%dx,	%dx
	xorw	%bx,	%bx
2:
	call	cmp32			# compare the dx:bx and inode.i_size
	jnc	goto_sys		# yes has gone to the end
	call	bmap			# convert the dx:bx into blocknr in ax

	call	read_block

	addw	$0x400,	%bp		# have gone to the end of segment
	jnc	1f
	movw	%es,	%bp
	addw	$0x1000, %bp		# next chunk
	movw	%bp,	%es
	xorw	%bp,	%bp		
1:
	# update the position
	addw	$0x400,	%bx
	adcw	$0,	%dx
	jmp	2b
goto_sys:
	ljmp	$SYSSEG, $0x00			

########## SUBROUTINE 	find_dir_entry
	# find the dir entry int es:bp
	# entry;
	#	es:bp	=>	address of buffer
	# return:
	#	CF	=	1 	found and ax is the inode number
	#	CF	=	0	not found
find_dir_entry:
	pushw	%si
	pushw	%di
	pushw	%bx
	pushw	%dx
	pushw	%cx

	xorw	%si,	%si
next_entry:
	cmpw	$1024,	%si
	jge	find_dir_entry.1
	call	strncmp
	jz	1f
	addw	$32,	%si	# next entry
	jmp	next_entry
1:
	stc			# have found	CF = 1
	movw	(%bp, %si,),	%ax
find_dir_entry.1:
	popw	%cx
	popw	%dx
	popw	%bx
	popw	%di
	popw	%si
	ret
	# entry:
	#	bp + si 	=>	address of source
strncmp:
	pushw	%si
	addw	%bp,	%si
	incw	%si
	incw	%si			# the offset of d_name  is two
	movw	$7,	%cx		# kernel + 0
	lea	kernel_name,	%di
	cld
	repz	cmpsb
	popw	%si
	ret				# ZF = 1 if success

########## SUBROUTINE 	cmp32
	# compare the position of file and file's size
	# entry : 
	#	dx:bx	=> position
	#	di	=> point to the inode
	# the size of file is in OFF_I_SIZE(%di)
	# return:
	# 	CF	=  1 	dx:bx <= filesize
	#	CF	=  0	dx:bx > filesize
cmp32:
	pushw	%dx
	shll	$16,	%edx
	movw	%bx,	%dx
	cmpl	OFF_I_SIZE(%di), %edx
	popw	%dx
	ret

######### SUBROUTINE 	bmap
	# convert logical offset to physical blocknr
	# entry:
	#	dx:bx	=>	offset
	#	di	=>	the address of inode
	# return:
	# 	ax	=>	the physical blocknr
bmap:
	pushw	%dx
	pushw	%bx
	pushw	%si
	pushw	%di
	pushw	%bp

	addw	$OFF_I_ZONE,	%di
	movw	%dx,	%ax
	shll	$16,	%eax
	movw	%bx,	%ax
	shrl	$10,	%eax
	# now ax is the logical block
	cmpw	$NR_DZONES,	%ax
	jae	1f
	shlw	$2,	%ax		# in direct block
	movw	%ax,	%bx
	movl	(%bx ,%di, 1),	%eax
	jmp	return
1:
	pushw	%es
	movw	$SETUPSEG, 	%bp
	movw	%bp,	%es		# now es must be SETUPSEG

	subw	$NR_DZONES,	%ax
	cmpw	$256,	%ax
	jae	1f			# in double indirect block
	# in indirect block
	subw	$1024,	%sp		# buffer
	movw	%sp,	%bp
	pushw	%ax
	movl	(NR_DZONES * 4)(%di),	%eax
	call	read_block
	popw	%ax
	shlw	$2,	%ax
	movw	%ax,	%si
	movl	(%bp, %si, 1),	%eax
	addw	$1024,	%sp
	popw	%es
	jmp	return
1:
	# double indirect block
	subw	$256,	%ax
	subw	$1024,	%sp
	movw	%sp,	%bp
	pushw	%ax
	movl	(NR_DZONES + 1) * 4 (%di),	%eax
	call	read_block
	popw	%ax
	xorw	%dx,	%dx
	movw	$256,	%bx
	divw	%bx
	shlw	$2,	%ax		# ax * 4
	movw	%ax,	%si
	movl	(%bp, %si, 1),	%eax	# now we have get the indirect block
	# we can free the previous buffer
	call	read_block
	shlw	$2,	%dx		# ax * 4
	movw	%dx,	%si
	movl	(%bp, %si, 1),	%eax
	addw	$1024,	%sp
	popw	%es
return:
	popw	%bp
	popw	%di
	popw	%si
	popw	%bx
	popw	%dx
	ret	
display:
	movw	$SETUPSEG, %ax
	movw	%ax,	%ds
1:	
	lodsb
	orb	%al,	%al
	movw	$0x7,	%bx		# page number, foreground color
	movb	$0xE,	%ah
	jz	die
	int	$0x10
	jmp	1b
die:
	jmp	die
	# subroutine : read one block into es:bp
	# entry: 
	# 	 ax ==> starting number of blocks 
	#	 es:bp ==> the buffer 
	# 	drive number is stored in drive_num variable
	# returned value:
	# 	CF = 1 , some error ocurred
	#	CF = 0 , successful
read_block:	
	pushw	%si
	pushw	%di
	pushl	%ebx
	pushw	%cx
	pushl	%edx
	movl	start_of_sect, %edx
	shll	$1,	%eax		# no * 2 to convert it to be sector unit
	addl	%edx,	%eax
	movl	%eax,	%ebx
	shrl	$16,	%ebx
	movw	$2,	%cx		# 2 sectors
	movb	drive_num,	%dl	# driver number
	call	read_disk
	popl	%edx
	popw	%cx
	popl	%ebx
	popw	%di
	popw	%si
	ret			

	# subroutine: read_disk to read data from hard disk
	# entry: ES:BP ==> memory address to read into
	# BX:AX	==> the obsolute number of sectors
	# CX	==> the number to be transfered
	# DL	==> driver number CAUTION: DL MUST BE GREATER THAN 0X80
	# First we try to read disk using the extension service
	# if it fails, we use the traditional int13 service
read_disk:
	pushw	%si
	pushw	%ds
	pushw	%ax	# obsolute block number
	pushl	$0
	pushw	%bx
	pushw	%ax	# starting obsolute block number
	pushw	%es
	pushw	%bp	# transfer buffer
	pushw	%cx
	pushw	$0x10	# 16 bytes
	movw	%sp,	%si
	pushw	%ss
	popw	%ds	# DS:SI point to Disk Address packet
	movb	$0x42,	%ah
#	jmp	test_trad	# ONLY TO TEST TRADITIONAL READ
	int	$0x13
	jnc	ret_main	# successful
test_trad:	
	addw	$0x10,	%sp	# have some errors
	popw	%ax
	popw	%ds
	popw	%si
	# has some error , using traditional method
	# first we should get the harddisk's parameter and 
	# convert the obsolute block number to C/H/S
	pushw	%cx	# parameters of entry
	pushw	%dx	
	pushw	%ax
	pushw	%bx
	movb	$0x8,	%ah
	int	$0x13
	# CH ==> low eight bits of maximum cylinder number
	# CL ==> Low six bits of maxismum setctor number
	#	 High two bits is high 2 bits of cylinder
	# DH ==> maximum head number
	movb	%dh,	%bl	# save the maximum head number
	# calculate the sector
	popw	%dx
	popw	%ax		# the obsolute block number
	andw	$0x3f,	%cx	# the low six bits is max sector number
	div	%cx
	movb	%dl,	%cl	# sector
	incb	%cl		# add one to correct sector
	# calcualte the head and cylinder
	xorw	%dx,	%dx
	xorb	%bh,	%bh
	incw	%bx		# maximum starts from 0
	div	%bx
	movb	%dl,	%dh	# head 
	# calculate the cylinder
	shl	$0x6,	%ah
	orb	%ah,	%cl
	movb	%al,	%ch	# cylinder
	popw	%ax
	movb	%al,	%dl	# disk number
	popw	%ax		# number to be transfered
	movb	$0x02,	%ah
	movw	%bp,	%bx	# tranfer buffer
	int	$0x13
	ret
ret_main:
	addw	$0x10,	%sp	# balance stack
	popw	%ax
	popw	%ds
	popw	%si
	ret

err_read_disk:
	.asciz	"Read harddisk Error!"
err_load_boot2:
	.asciz	"Load Boot2 Error!"
err_no_sig:
	.asciz	"The boot2 dosen't have signature 0xA5A5!"
err_not_read_super:
	.asciz	"Can not read superblock!"
err_not_read_root_inode:
	.asciz	"Can not read root inode!"
err_read_root_dir:
	.asciz  "Can not read root dir!"
err_root_too_many_entries:
	.asciz	"root entry is too large"
err_no_img:
	.asciz	"no img in hard"
err_no_magic:
	.asciz	"no magic in superblock!"

kernel_name:
	.asciz	"kernel"
start_of_sect:
	.word	0
	.word	0
nr_of_sect:
	.word	0
	.word	0
drive_num:
	.word	0x80		# default is the first hard disk
.org	1024 - 2
	.word	0xA5A5

/*
 * $Log: boot2.s,v $
 * Revision 1.2  2003/10/08 09:44:10  cnqin
 * Add $id$ and $log$ keyword
 *
 */
