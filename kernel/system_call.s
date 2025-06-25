
.extern printk
.global timer_interrupt

.align 2
timer_interrupt:
    pushl %eax
    pushl %ecx
    pushl %edx
    # 保存段寄存器，普通函数调用不需要保存段寄存器，中断可能发生在用户模式（DS/ES/FS不同）
    push %ds
    push %es
    push %fs
    # 设置内核数据段
    movl $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    # 打印信息
    push $msg
    call printk
    popl %eax   # 清理栈 (平衡push $msg)
    # 恢复寄存器
    pop %fs
    pop %es
    pop %ds
    popl %edx
    popl %ecx
    popl %eax
    # 由于初始化8259A中断控制芯片时没有采用自动EOI，所以这里需要发指令结束该硬件中断
    movb $0x20, %al
    outb %al, $0x20
    iret


msg:
    .asciz "timer_interrupt handler is called\n"
