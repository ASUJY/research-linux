
.globl divide_error,debug,nmi,int3,overflow,bounds,invalid_op
.globl double_fault,coprocessor_segment_overrun
.globl invalid_TSS,segment_not_present,stack_segment
.globl general_protection,coprocessor_error,irq13,reserved

divide_error:
    pushl $do_divide_error
/*
* 用于处理无错误码的中断（CPU未自动压入错误码）
* 核心功能是保存被中断任务的上下文、调用中断处理函数，然后恢复上下文并返回
*/
no_error_code:
    xchgl %eax, (%esp)  /* eax寄存器和esp指向的地址交换内容，eax=中断处理函数的地址，(esp)=原始eax值 */
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %edi
    pushl %esi
    pushl %ebp
    push %ds
    push %es
    push %fs

    pushl $0            /* 无错误码的中断需压入0，保持栈结构统一（与有错误码中断的栈布局一致）。 */
    lea 44(%esp), %edx  /* 计算栈指针 %esp 上方 44 字节处的地址，并将该地址值存入 %edx 寄存器中，类似于C语言中的 &abc(取地址) */
    pushl %edx
    movl $0x10, %edx
    mov %dx, %ds
    mov %dx, %es
    mov %dx, %fs
    call *%eax      /* 调用中断处理函数 */
    addl $8, %esp   /* 平栈 */

    pop %fs
    pop %es
    pop %ds
    popl %ebp
    popl %esi
    popl %edi
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax
    iret

debug:
	pushl $do_int3		# _do_debug
	jmp no_error_code

nmi:
	pushl $do_nmi
	jmp no_error_code

int3:
	pushl $do_int3
	jmp no_error_code

overflow:
	pushl $do_overflow
	jmp no_error_code

bounds:
	pushl $do_bounds
	jmp no_error_code

invalid_op:
	pushl $do_invalid_op
	jmp no_error_code

coprocessor_segment_overrun:
	pushl $do_coprocessor_segment_overrun
	jmp no_error_code

reserved:
    pushl $do_reserved
    jmp no_error_code

irq13:
	pushl %eax
	xorb %al,%al
	outb %al,$0xF0
	movb $0x20,%al
	outb %al,$0x20
	jmp 1f
1:	jmp 1f
1:	outb %al,$0xA0
	popl %eax
	jmp coprocessor_error

double_fault:
	pushl $do_double_fault
error_code:
	xchgl %eax,4(%esp)		# error code <-> %eax
	xchgl %ebx,(%esp)		# &function <-> %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds
	push %es
	push %fs
	pushl %eax			# error code
	lea 44(%esp),%eax		# offset
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	call *%ebx
	addl $8,%esp
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret

invalid_TSS:
	pushl $do_invalid_TSS
	jmp error_code

segment_not_present:
	pushl $do_segment_not_present
	jmp error_code

stack_segment:
	pushl $do_stack_segment
	jmp error_code

general_protection:
	pushl $do_general_protection
	jmp error_code