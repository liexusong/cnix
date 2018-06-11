need_sched = 0
signal	   = 4
esp_off    = 8
eip_off    = 12
esp0_off   = 16
pg_dir_off = 20

nr_syscall = 256
ENOSYS = 38
EFLAGS	=	48
CS	=	44
EIP	=	40
INDEX	=	36
ES	=	32
DS	=	28
EAX	=	24
EBP	=	20
EDI	=	16
ESI 	=	12
EDX	=	8
ECX	=	4
EBX	=	0

#define SAVE_SPOT	\
	cld;		\
	pushl	%es;	\
	pushl	%ds;	\
	pushl	%eax;	\
	pushl	%ebp;	\
	pushl	%edi;	\
	pushl	%esi;	\
	pushl	%edx;	\
	pushl	%ecx;	\
	pushl	%ebx;	\
	movl	$0x10, %edx;	\
	movl	%edx, %ds;	\
	movl	%edx, %es;	\
	
#define RESTORE_SPOT	\
	popl	%ebx;	\
	popl	%ecx;	\
	popl	%edx;	\
	popl	%esi;	\
	popl	%edi;	\
	popl	%ebp;	\
	popl	%eax;	\
	popl	%ds;	\
	popl	%es;	\

.global system_call

.global hw_int0, hw_int1, hw_int2, hw_int3, hw_int4, hw_int5, hw_int6, hw_int7
.global hw_int8, hw_int9, hw_int10, hw_int11, hw_int12, hw_int13, hw_int14, 
.global hw_int15

.global divide_err, debug, nmi, int3, overflow, bounds, invalid_op
.global device_not_avl, double_fault, coprocessor_segment_overrun, invalid_tss
.global segment_not_present, stack_segment, general_protection, page_fault
.global coprocessor_fault

.global ret_from_syscall
.global restore_all

.global switch_to
	
	.align 2
bad_syscall:
	movl	$-ENOSYS, EAX(%esp)
	jmp	ret_from_syscall
	
	.align 2
system_call:
	pushl	%eax
	SAVE_SPOT
	cmpl	$nr_syscall - 1, %eax
	ja	bad_syscall

	sti

	call	*syscall_table(, %eax, 4)
	movl	%eax, EAX(%esp)
#if 0
	call syscall_print
#endif

ret_from_syscall:
	call	do_with_bottom

	/* syscall from kernel */
	movl	CS(%esp), %eax
	testl	$0x03, %eax
	je	restore_all

ret_with_reschedule:
	movl	current, %ebx
	cmpl	$0, need_sched(%ebx)
	jne	reschedule

check_signal:
	cmpl	$0, signal(%ebx)
	je	restore_all

	call	do_with_signal

restore_all:
	cli

	RESTORE_SPOT
	addl	$4, %esp
	iret

reschedule:
	call	schedule
	
	jmp	check_signal
	
ret_from_intr:
	cmpl	$0, nested_intnum
	jne	restore_all

	call	do_with_bottom

	movl	CS(%esp), %eax
	testl	$0x03, %eax
	jne	ret_with_reschedule

	jmp	restore_all
	
	.align 4
hw_int0:
	pushl	$0
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int1:
	pushl	$1
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int2:
	pushl	$2
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int3:
	pushl	$3
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int4:
	pushl	$4
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int5:
	pushl	$5
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int6:
	pushl	$6
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int7:
	pushl	$7
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int8:
	pushl	$8
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int9:
	pushl	$9
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int10:
	pushl	$10
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int11:
	pushl	$11
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int12:
	pushl	$12
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int13:
	pushl	$13
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int14:
	pushl	$14
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr

	.align 4
hw_int15:
	pushl	$15
	SAVE_SPOT
	call	do_with_irq
	jmp ret_from_intr
	
	.align 4
divide_err:
	pushl	$0
	pushl	$0
	jmp	execption
	
	.align 4
debug:
	pushl	$0
	pushl	$1
	jmp	execption
	
	.align 4
nmi:
	pushl	$0
	pushl	$2
	jmp	execption
	
	.align 4
int3:
	pushl	$0
	pushl	$3
	jmp	execption
	
	.align 4
overflow:
	pushl	$0
	pushl	$4
	jmp	execption

	.align 4
bounds:
	pushl	$0
	pushl	$5
	jmp	execption

	.align 4
invalid_op:
	pushl	$0
	pushl	$6
	jmp	execption
	
	.align 4
device_not_avl:
	pushl	$0
	pushl	$7
	jmp	execption
	
	.align 4
double_fault:
	pushl	$8
	jmp	execption
	
	.align 4
coprocessor_segment_overrun:
	pushl	$0
	pushl	$9
	jmp	execption
	
	.align 4
invalid_tss:
	pushl	$10
	jmp	execption
	
	.align 4
segment_not_present:
	pushl	$11
	jmp	execption
	
	.align 4
stack_segment:
	pushl	$12
	jmp	execption
	
	.align 4
general_protection:
	pushl	$13
	jmp	execption
	
	.align 4
page_fault:	
	pushl	$14
	jmp	execption
	
	.align 4
coprocessor_fault:
	pushl	$0
	pushl	$15
	jmp	execption
	

execption:
	cld		
	pushl	%ds
	pushl	%eax
	pushl	%ebp
	pushl	%edi
	pushl	%esi
	pushl	%edx
	pushl	%ecx
	pushl	%ebx
	movl	$0x10, %eax
	movl	%eax, %ds
	call	do_with_execption
	popl	%ebx
	popl	%ecx
	popl	%edx
	popl	%esi
	popl	%edi
	popl	%ebp
	popl	%eax
	popl	%ds
	addl	$8, %esp
	iret

switch_to:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %ecx
	pushl %esi
	pushl %edi
	pushl %fs
	pushl %gs

	movl 8(%ebp), %ebx
	movl %esp, esp_off(%ebx) /* save prev->esp */
	movl $1f, eip_off(%ebx) /* save prev->eip */

	movl 12(%ebp), %ebx
	movl esp_off(%ebx), %esp /* restore next->esp */
	pushl eip_off(%ebx)

	movl esp0_off(%ebx), %eax
	movl 16(%ebp), %ebx

	cmpl $0, %ebx
	je   pauselife

	movl %eax, 4(%ebx) /* save next->esp0 to tss.esp0 */

	movl 12(%ebp), %ebx
	movl pg_dir_off(%ebx), %eax
	movl %eax, %cr3 /* resotre next->pg_dir to cr3 */

pauselife:
	ret

	1:
	popl %gs
	popl %fs
	popl %edi
	popl %esi
	popl %ecx
	popl %ebx
	popl %ebp
	ret
