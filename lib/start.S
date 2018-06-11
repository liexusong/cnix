.text
.global start
start:
	xor    %ebp,%ebp
	pop    %esi
	mov    %esp,%ecx
	and    $0xfffffff0,%esp
	push   %eax
	push   %esp
	push   %edx
	push   $0
	push   $0
	push   %ecx
	push   %esi
	push   $main
	call   libc_start_main
	hlt 
	nop 
	nop 
