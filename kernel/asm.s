
.globl divide_error
.globl reserved

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

reserved:
    pushl $do_reserved
    jmp no_error_code
