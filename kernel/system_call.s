
# 堆栈中各个寄存器的偏移位置
CS		= 0x20


.global timer_interrupt

ret_from_sys_call:
    movl current, %eax  # task[0] cannot have signals
    cmpl task, %eax     # 直接引用task相当于引用task[0]
    je 3f
    #通过对原调用程序代码段选择子的检查来判断调用程序是否是内核任务（例如任务1）。如果是则直接
    #退出中断。否则对于普通进程则需进行信号量的处理。现在暂时不处理！

    # 恢复上下文
3:	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
	pop %fs
	pop %es
	pop %ds
	iret

# 时钟中断处理程序。jiffies(滴答数)每10毫秒加1
.align 2
timer_interrupt:
    # 保存段寄存器，普通函数调用不需要保存段寄存器，中断可能发生在用户模式，所以这里需要保存（DS/ES/FS不同）
    push %ds    # save ds,es and put kernel data space
    push %es    # into them. %fs is used by _system_call
    push %fs

    pushl %edx		# we save %eax,%ecx,%edx as gcc doesn't
    pushl %ecx		# save those across function calls. %ebx
    pushl %ebx		# is saved as we use that in ret_sys_call
    pushl %eax

    # 设置内核数据段
    movl $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    # 设置fs段寄存器为LDT表中的第二个描述符（进程自己的数据段）
    movl $0x17,%eax
    mov %ax,%fs

    incl jiffies

    # 由于初始化中断控制芯片(8259A)时没有采用自动EOI，所以这里需要发指令结束该硬件中断(时钟中断)
    movb $0x20, %al
    outb %al, $0x20

    movl CS(%esp), %eax     # 用当前特权级作为参数调用do_timer
    andl $3, %eax           # %eax is CPL (0 or 3, 0=supervisor)
    pushl %eax
    call do_timer           # 'do_timer(long CPL)' 执行任务切换，计时等工作
    addl $4, %esp
    jmp ret_from_sys_call
