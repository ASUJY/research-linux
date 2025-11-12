
# 堆栈中各个寄存器的偏移位置
EIP		= 0x1C
CS		= 0x20
OLDSS		= 0x2C

# task_struct中变量的偏移值
state	= 0
counter	= 4
signal	= 12
blocked = (33*16)

nr_system_calls = 72    # 系统调用总数量

.global system_call,sys_fork,timer_interrupt,sys_execve
.global hd_interrupt

# 系统调用出错，设置错误码为-1
.align 2
bad_sys_call:
    movl $-1, %eax
    iret

# 重新执行调度程序
.align 2
reschedule:
	pushl $ret_from_sys_call
	jmp schedule

# 0x80系统调用入口点
.align 2
system_call:
    cmpl $nr_system_calls - 1, %eax # 调用号如果超出范围的话，就调用bad_sys_call
    ja bad_sys_call

    push %ds
    push %es
    push %fs
    pushl %edx
    pushl %ecx		# push %ebx,%ecx,%edx as parameters to the system call
    pushl %ebx

    # 设置ds,es为内核数据段选择子
    movl $0x10, %edx
    mov %dx, %ds
    mov %dx, %es
    # 设置fs段寄存器为LDT表中的第二个描述符（进程自己的数据段）
    movl $0x17, %edx
    mov %dx, %fs
    # 调用[sys_call_table + %eax * 4]指向的系统调用函数
    call *sys_call_table(,%eax,4)
    pushl %eax
    # 如果当前任务的运行状态不是就绪态(state不等于0)或者时间片已用完(counter=0)，则去执行调度程序
    movl current, %eax
    cmpl $0, state(%eax)		# state
    jne reschedule
    cmpl $0, counter(%eax)		# counter
    je reschedule

# 从内核态返回到用户态的操作(硬件中断，系统调用)最后都要执行这个函数
ret_from_sys_call:
    movl current, %eax  # task[0] cannot have signals
    cmpl task, %eax     # 直接引用task相当于引用task[0]
    je 3f
    #通过对原调用程序代码段选择子的检查来判断调用程序是否是内核任务（例如任务1）。如果是则直接
    #退出中断。否则对于普通进程则需进行信号量的处理。现在暂时不处理！
    cmpw $0x0f,CS(%esp)		# was old code segment supervisor ?
    jne 3f
    cmpw $0x17,OLDSS(%esp)		# was stack segment = 0x17 ?
    jne 3f
    movl signal(%eax),%ebx
    movl blocked(%eax),%ecx
    notl %ecx
    andl %ebx,%ecx
    bsfl %ecx,%ecx
    je 3f

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

.align 2
sys_execve:
	lea EIP(%esp),%eax
	pushl %eax
	call do_execve
	addl $4,%esp
	ret

.align 2
sys_fork:
	call find_empty_process
	testl %eax, %eax
	js 1f
	push %gs
	pushl %esi
	pushl %edi
	pushl %ebp
	pushl %eax
	call copy_process
	addl $20, %esp
1:	ret

hd_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	movl $0x17, %eax
	mov %ax, %fs
	movb $0x20, %al
	outb %al, $0xA0		# EOI to interrupt controller #1
	jmp 1f			# give port chance to breathe
1:	jmp 1f
1:	xorl %edx, %edx
	xchgl do_hd, %edx
	testl %edx, %edx
	jne 1f
	movl $unexpected_hd_interrupt, %edx
1:	outb %al, $0x20
	call *%edx		# "interesting" way of handling intr.
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret